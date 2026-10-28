// Connection profiles: the title bar's radio strip shows these in place of
// discovered radios once one exists. See core/ConnectionProfiles.h.

#include "MainWindow.h"

#include "ConnectionPanel.h"
#include "ConnectionProfilesDialog.h"
#include "FramelessMessageBox.h"
#include "TitleBar.h"
#include "core/AppSettings.h"
#include "core/LogManager.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QJsonDocument>
#include <QStatusBar>
#include <QTimer>

namespace AetherSDR {

namespace {

const QString kProfileTabPrefix = QStringLiteral("profile:");

QString routeText(const ConnectionProfile& p)
{
    switch (p.route) {
    case ConnectionProfile::Route::SmartLink: return QStringLiteral("SmartLink");
    case ConnectionProfile::Route::Ip:        return p.address;
    case ConnectionProfile::Route::Lan:       break;
    }
    return QStringLiteral("LAN");
}

} // namespace

void MainWindow::wireConnectionProfiles()
{
    loadConnectionProfiles();

    connect(&m_radioModel, &RadioModel::connectionStateChanged, this, [this](bool connected) {
        if (connected) {
            m_profileSessionLive = true;
            if (!m_applyProfileId.isEmpty()) {
                const QString id = m_applyProfileId;
                m_applyProfileId.clear();
                // Let the session settle (slices, profile lists) before writing to it.
                QTimer::singleShot(1500, this, [this, id] { applyConnectionProfileSettings(id); });
            }
        } else if (m_profileSessionLive) {
            m_profileSessionLive = false;
            m_activeProfileId.clear();
            if (!m_pendingProfileId.isEmpty()) {
                const QString next = m_pendingProfileId;
                m_pendingProfileId.clear();
                QTimer::singleShot(800, this, [this, next] {
                    if (const ConnectionProfile* p = m_connectionProfiles.find(next))
                        connectToConnectionProfile(*p);
                });
            }
        }
        scheduleRadioTabRefresh();
    });
}

void MainWindow::loadConnectionProfiles()
{
    const QByteArray raw = AppSettings::instance()
        .value(QString::fromLatin1(ConnectionProfileSet::kSettingsKey), QStringLiteral("{}"))
        .toString().toUtf8();
    m_connectionProfiles = ConnectionProfileSet::fromJson(QJsonDocument::fromJson(raw).object());
}

void MainWindow::saveConnectionProfiles()
{
    auto& settings = AppSettings::instance();
    settings.setValue(QString::fromLatin1(ConnectionProfileSet::kSettingsKey),
                      QString::fromUtf8(QJsonDocument(m_connectionProfiles.toJson())
                                            .toJson(QJsonDocument::Compact)));
    settings.save();
}

ConnectionSessionIdentity MainWindow::currentSessionIdentity() const
{
    ConnectionSessionIdentity s;
    s.connected = m_radioModel.isConnected();
    if (!s.connected)
        return s;
    s.wan = m_radioModel.isWan();
    s.family = m_radioModel.family();
    s.serial = m_radioModel.serial();
    const RadioInfo info = m_radioModel.lastRadioInfo();
    if (!info.address.isNull())
        s.address = info.address.toString();
    return s;
}

QString MainWindow::liveConnectionProfileId() const
{
    const ConnectionSessionIdentity session = currentSessionIdentity();
    if (!session.connected)
        return QString();
    if (!m_activeProfileId.isEmpty() && m_connectionProfiles.find(m_activeProfileId))
        return m_activeProfileId;
    // Connected some other way (picker, auto-connect, reconnect): light the
    // profile that describes this session, if one does.
    const ConnectionProfile* match = m_connectionProfiles.matchSession(session);
    return match ? match->id : QString();
}

QList<RadioTabEntry> MainWindow::connectionProfileTabs() const
{
    const QString live = liveConnectionProfileId();
    QList<RadioTabEntry> tabs;
    for (const ConnectionProfile& p : m_connectionProfiles.profiles()) {
        RadioTabEntry entry;
        entry.id = kProfileTabPrefix + p.id;
        entry.name = p.name;
        // Line two reads "Connected · SmartLink": the name already says which
        // radio, so the model would only crowd out the route.
        entry.detail = routeText(p);
        entry.transport = p.radioLabel.isEmpty() ? routeText(p)
                                                  : p.radioLabel + QStringLiteral(" · ") + routeText(p);
        entry.status = p.id == live ? RadioTabStatus::Connected : RadioTabStatus::Available;
        tabs.append(entry);
    }
    return tabs;
}

void MainWindow::onConnectionProfileActivated(const QString& profileId)
{
    const ConnectionProfile* target = m_connectionProfiles.find(profileId);
    if (!target)
        return;
    const QString targetName = target->name;
    const QString live = liveConnectionProfileId();

    switch (decideProfileClick(m_radioModel.isConnected(), live, profileId)) {
    case ProfileClickAction::Connect:
        connectToConnectionProfile(*target);
        return;
    case ProfileClickAction::ConfirmDisconnect: {
        const auto answer = FramelessMessageBox::question(
            this, tr("Disconnect"), tr("Disconnect from \"%1\"?").arg(targetName),
            FramelessMessageBox::Yes | FramelessMessageBox::No, FramelessMessageBox::No);
        if (answer != FramelessMessageBox::Yes)
            return;
        m_pendingProfileId.clear();
        m_connPanel->automationDisconnect();
        return;
    }
    case ProfileClickAction::ConfirmSwitch: {
        QString from;
        if (const ConnectionProfile* current = m_connectionProfiles.find(live))
            from = current->name;
        else
            from = m_radioModel.nickname().isEmpty() ? m_radioModel.model()
                                                     : m_radioModel.nickname();
        const auto answer = FramelessMessageBox::question(
            this, tr("Switch profile"),
            tr("Switch from \"%1\" to \"%2\"?\n\n\"%1\" will be disconnected first.")
                .arg(from, targetName),
            FramelessMessageBox::Yes | FramelessMessageBox::No, FramelessMessageBox::No);
        if (answer != FramelessMessageBox::Yes)
            return;
        m_pendingProfileId = profileId;
        m_connPanel->automationDisconnect();
        return;
    }
    }
}

bool MainWindow::connectToConnectionProfile(const ConnectionProfile& profile)
{
    QString error;
    bool started = false;
    m_activeProfileId = profile.id;
    m_applyProfileId = profile.appliesSettings() ? profile.id : QString();

    switch (profile.route) {
    case ConnectionProfile::Route::Lan:
        started = m_connPanel->automationConnectLocalSerial(profile.serial, &error);
        // Not discovered (another subnet, or not announced yet): the address
        // the profile was saved with still reaches it.
        if (!started && !profile.address.isEmpty())
            started = m_connPanel->automationConnectByIp(profile.address, profile.family, &error);
        break;
    case ConnectionProfile::Route::Ip:
        started = m_connPanel->automationConnectByIp(profile.address, profile.family, &error);
        break;
    case ConnectionProfile::Route::SmartLink:
        for (const WanRadioInfo& wan : std::as_const(m_smartLinkRadios)) {
            if (wan.serial.compare(profile.serial, Qt::CaseInsensitive) == 0) {
                startWanRadioConnect(wan, true);
                started = true;
                break;
            }
        }
        if (!started)
            error = tr("the radio isn't in the SmartLink list — log in to SmartLink first");
        break;
    }

    if (!started) {
        m_activeProfileId.clear();
        m_applyProfileId.clear();
        qCWarning(lcProtocol) << "MainWindow: connection profile" << profile.name
                              << "could not start:" << error;
        statusBar()->showMessage(tr("Can't connect \"%1\": %2").arg(profile.name, error), 8000);
        scheduleRadioTabRefresh();
        return false;
    }
    statusBar()->showMessage(tr("Connecting \"%1\"…").arg(profile.name), 4000);
    scheduleRadioTabRefresh();
    return true;
}

void MainWindow::applyConnectionProfileSettings(const QString& profileId, int attempt)
{
    const ConnectionProfile* found = m_connectionProfiles.find(profileId);
    if (!found || !m_radioModel.isConnected())
        return;
    const ConnectionProfile p = *found;
    const bool flex = m_radioModel.family() == QLatin1String("flex");

    auto applySlice = [this, p](int tries, auto&& self) -> void {
        if (!m_radioModel.isConnected())
            return;
        if (p.frequencyMhz <= 0.0 && p.mode.isEmpty() && p.rfPower < 0)
            return;
        SliceModel* slice = activeSlice();
        if (!slice && !m_radioModel.slices().isEmpty())
            slice = m_radioModel.slices().first();
        if (!slice && (p.frequencyMhz > 0.0 || !p.mode.isEmpty())) {
            // The radio has not reported a slice yet; give it a few seconds.
            if (tries < 20)
                QTimer::singleShot(250, this, [self, tries] { self(tries + 1, self); });
            return;
        }
        if (slice && p.frequencyMhz > 0.0)
            slice->tuneAndRecenter(p.frequencyMhz);
        if (slice && !p.mode.isEmpty()) {
            const QStringList modes = slice->modeList();
            if (modes.isEmpty() || modes.contains(p.mode, Qt::CaseInsensitive))
                slice->setMode(p.mode);
            else
                statusBar()->showMessage(
                    tr("Profile \"%1\": mode %2 isn't offered by this radio").arg(p.name, p.mode),
                    8000);
        }
        if (p.rfPower >= 0)
            m_radioModel.transmitModel().setRfPower(p.rfPower);
        qCInfo(lcProtocol) << "MainWindow: applied connection profile" << p.name;
    };

    if (!flex) {
        applySlice(attempt, applySlice);
        return;
    }
    // Flex: Global first (it rebuilds slices and may carry its own TX
    // profile), then TX, then any explicit slice values on top.
    if (!p.flexGlobalProfile.isEmpty())
        m_radioModel.loadGlobalProfile(p.flexGlobalProfile);
    const int txDelay = p.flexGlobalProfile.isEmpty() ? 0 : 1500;
    QTimer::singleShot(txDelay, this, [this, p] {
        if (m_radioModel.isConnected() && !p.flexTxProfile.isEmpty())
            m_radioModel.transmitModel().loadProfile(p.flexTxProfile);
    });
    QTimer::singleShot(txDelay + 1000, this, [applySlice] { applySlice(0, applySlice); });
}

void MainWindow::showConnectionProfilesDialog()
{
    ConnectionProfilesDialog::Context context;
    const ConnectionSessionIdentity session = currentSessionIdentity();
    if (session.connected) {
        ConnectionProfile& c = context.current;
        c.family = session.family;
        c.serial = session.serial;
        c.address = session.address;
        c.radioLabel = m_radioModel.model();
        c.name = m_radioModel.nickname().isEmpty() ? m_radioModel.model() : m_radioModel.nickname();
        if (session.wan) {
            c.route = ConnectionProfile::Route::SmartLink;
        } else {
            bool discovered = false;
            for (const RadioInfo& info : m_connPanel->automationLocalRadios())
                discovered = discovered || (!c.serial.isEmpty() && info.serial == c.serial);
            c.route = discovered || c.address.isEmpty() ? ConnectionProfile::Route::Lan
                                                        : ConnectionProfile::Route::Ip;
        }
        if (session.family == QLatin1String("flex")) {
            context.flexGlobalProfiles = m_radioModel.globalProfiles();
            context.flexTxProfiles = m_radioModel.transmitModel().profileList();
        }
        if (SliceModel* slice = activeSlice())
            context.modes = slice->modeList();
    }
    for (const RadioInfo& info : m_connPanel->automationLocalRadios()) {
        ConnectionProfile r;
        r.route = ConnectionProfile::Route::Lan;
        r.family = info.family.isEmpty() ? QStringLiteral("flex") : info.family.toLower();
        r.serial = info.serial;
        r.address = info.address.isNull() ? QString() : info.address.toString();
        r.radioLabel = info.model.isEmpty() ? info.name : info.model;
        r.name = info.nickname.isEmpty() ? r.radioLabel : info.nickname;
        if (r.isReachable())
            context.knownRadios.append(r);
    }
    for (const WanRadioInfo& wan : std::as_const(m_smartLinkRadios)) {
        ConnectionProfile r;
        r.route = ConnectionProfile::Route::SmartLink;
        r.serial = wan.serial;
        r.radioLabel = wan.model;
        r.name = wan.nickname.isEmpty() ? wan.model : wan.nickname;
        if (r.isReachable())
            context.knownRadios.append(r);
    }
    // A radio reached by IP (Icom, a routed Flex) is never discovered; offer
    // the live one so it can be picked too.
    if (session.connected && !session.wan && !session.address.isEmpty()) {
        bool listed = false;
        for (const ConnectionProfile& r : std::as_const(context.knownRadios))
            listed = listed || r.address == session.address;
        if (!listed) {
            ConnectionProfile r = context.current;
            r.route = ConnectionProfile::Route::Ip;
            context.knownRadios.append(r);
        }
    }
    if (context.modes.isEmpty()) {
        context.modes = {QStringLiteral("USB"), QStringLiteral("LSB"), QStringLiteral("CW"),
                         QStringLiteral("DIGU"), QStringLiteral("DIGL"), QStringLiteral("AM"),
                         QStringLiteral("FM"), QStringLiteral("RTTY")};
    }

    auto* dialog = new ConnectionProfilesDialog(m_connectionProfiles, context, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    trackPersistentDialog(dialog);
    connect(dialog, &ConnectionProfilesDialog::profilesChanged, this,
            [this](const ConnectionProfileSet& profiles) {
        m_connectionProfiles = profiles;
        if (!m_connectionProfiles.find(m_activeProfileId))
            m_activeProfileId.clear();
        saveConnectionProfiles();
        scheduleRadioTabRefresh();
    });
    dialog->show();
}

} // namespace AetherSDR
