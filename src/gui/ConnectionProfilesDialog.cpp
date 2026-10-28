#include "ConnectionProfilesDialog.h"

#include "core/ThemeManager.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace AetherSDR {

namespace {

QComboBox* editableChoices(const QStringList& items, QWidget* parent)
{
    auto* combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->addItem(QString());   // "leave alone"
    combo->addItems(items);
    return combo;
}

QString routeLabel(ConnectionProfile::Route route)
{
    switch (route) {
    case ConnectionProfile::Route::Ip:        return QObject::tr("IP address");
    case ConnectionProfile::Route::SmartLink: return QObject::tr("SmartLink");
    case ConnectionProfile::Route::Lan:       break;
    }
    return QObject::tr("LAN (discovered)");
}

} // namespace

ConnectionProfilesDialog::ConnectionProfilesDialog(const ConnectionProfileSet& profiles,
                                                   const Context& context, QWidget* parent)
    : PersistentDialog(tr("Connection Profiles"),
                       QStringLiteral("ConnectionProfilesDialogGeometry"), parent)
    , m_profiles(profiles)
    , m_context(context)
{
    setObjectName(QStringLiteral("connectionProfilesDialog"));
    auto* root = new QHBoxLayout(bodyWidget());

    auto* left = new QVBoxLayout;
    m_list = new QListWidget(bodyWidget());
    m_list->setObjectName(QStringLiteral("connectionProfilesList"));
    m_list->setAccessibleName(tr("Connection profiles"));
    m_list->setMinimumWidth(180);
    left->addWidget(m_list, 1);
    auto* fromCurrent = new QPushButton(tr("New from current connection"), bodyWidget());
    fromCurrent->setObjectName(QStringLiteral("connectionProfileFromCurrent"));
    fromCurrent->setEnabled(!m_context.current.serial.isEmpty()
                            || !m_context.current.address.isEmpty());
    auto* blank = new QPushButton(tr("New"), bodyWidget());
    m_delete = new QPushButton(tr("Delete"), bodyWidget());
    m_delete->setObjectName(QStringLiteral("connectionProfileDelete"));
    left->addWidget(fromCurrent);
    auto* row = new QHBoxLayout;
    row->addWidget(blank);
    row->addWidget(m_delete);
    left->addLayout(row);
    root->addLayout(left);

    auto* right = new QVBoxLayout;
    auto* form = new QFormLayout;
    m_name = new QLineEdit(bodyWidget());
    m_name->setObjectName(QStringLiteral("connectionProfileName"));
    m_name->setPlaceholderText(tr("e.g. MSHV, JTDX-VU"));
    form->addRow(tr("Name"), m_name);

    // Type first: it decides which routes exist, and the route decides which
    // radios the list offers.
    m_family = new QComboBox(bodyWidget());
    m_family->setObjectName(QStringLiteral("connectionProfileFamily"));
    m_family->addItem(QStringLiteral("FlexRadio"), QStringLiteral("flex"));
    m_family->addItem(QStringLiteral("Icom"), QStringLiteral("icom"));
    m_family->addItem(QStringLiteral("Hermes-Lite 2"), QStringLiteral("hl2"));
    m_family->addItem(QStringLiteral("ANAN"), QStringLiteral("anan"));
    form->addRow(tr("Radio type"), m_family);

    m_route = new QComboBox(bodyWidget());
    m_route->setObjectName(QStringLiteral("connectionProfileRoute"));
    form->addRow(tr("Connect via"), m_route);

    m_radioPick = new QComboBox(bodyWidget());
    m_radioPick->setObjectName(QStringLiteral("connectionProfileRadio"));
    m_radioPick->setAccessibleName(tr("Radio"));
    form->addRow(tr("Radio"), m_radioPick);

    m_serial = new QLineEdit(bodyWidget());
    m_serial->setPlaceholderText(tr("filled in when you choose a radio"));
    form->addRow(tr("Serial"), m_serial);
    m_address = new QLineEdit(bodyWidget());
    m_address->setPlaceholderText(QStringLiteral("192.168.1.148"));
    form->addRow(tr("Address"), m_address);
    m_form = form;

    auto* applyHeader = new QLabel(tr("After connecting (blank = leave as is)"), bodyWidget());
    applyHeader->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
    form->addRow(applyHeader);
    m_flexGlobal = editableChoices(m_context.flexGlobalProfiles, bodyWidget());
    form->addRow(tr("Flex Global profile"), m_flexGlobal);
    m_flexTx = editableChoices(m_context.flexTxProfiles, bodyWidget());
    form->addRow(tr("Flex TX profile"), m_flexTx);

    m_frequency = new QDoubleSpinBox(bodyWidget());
    m_frequency->setDecimals(6);
    m_frequency->setRange(0.0, 100000.0);
    m_frequency->setSuffix(QStringLiteral(" MHz"));
    m_frequency->setSpecialValueText(tr("Leave as is"));
    form->addRow(tr("Frequency"), m_frequency);
    m_mode = editableChoices(m_context.modes, bodyWidget());
    form->addRow(tr("Mode"), m_mode);
    m_power = new QSpinBox(bodyWidget());
    m_power->setRange(-1, 100);
    m_power->setSuffix(QStringLiteral(" %"));
    m_power->setSpecialValueText(tr("Leave as is"));
    form->addRow(tr("RF power"), m_power);
    right->addLayout(form);

    m_status = new QLabel(bodyWidget());
    m_status->setWordWrap(true);
    m_status->setVisible(false);
    right->addWidget(m_status);
    right->addStretch(1);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* save = new QPushButton(tr("Save"), bodyWidget());
    save->setObjectName(QStringLiteral("connectionProfileSave"));
    save->setDefault(true);
    auto* close = new QPushButton(tr("Close"), bodyWidget());
    buttons->addWidget(save);
    buttons->addWidget(close);
    right->addLayout(buttons);
    root->addLayout(right, 1);

    ThemeManager::instance().applyStyleSheet(this, QStringLiteral(
        "QDialog, QLabel { background: {{color.background.1}}; color: {{color.text.primary}}; }"
        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QListWidget {"
        " background: {{color.background.0}}; color: {{color.text.primary}};"
        " border: 1px solid {{color.border.strong}}; padding: 4px; }"
        "QPushButton { background: {{color.background.2}}; color: {{color.text.primary}}; padding: 6px 14px; }"
        "QPushButton:disabled { color: {{color.text.secondary}}; }"
        "QPushButton:focus, QLineEdit:focus { border: 1px solid {{color.border.accent}}; }"));

    connect(m_family, &QComboBox::currentIndexChanged, this, [this] { rebuildRoutes(); });
    connect(m_route, &QComboBox::currentIndexChanged, this, &ConnectionProfilesDialog::updateRouteFields);
    connect(m_radioPick, &QComboBox::activated, this, [this](int index) {
        const QVariant data = m_radioPick->itemData(index);
        const int known = data.isValid() ? data.toInt() : -1;
        if (known < 0 || known >= m_context.knownRadios.size())
            return;
        const ConnectionProfile& r = m_context.knownRadios.at(known);
        const bool ip = currentRoute() == ConnectionProfile::Route::Ip;
        m_serial->setText(ip ? QString() : r.serial);
        m_address->setText(r.address);
        m_pickedRadioLabel = r.radioLabel;
        if (m_name->text().trimmed().isEmpty())
            m_name->setText(r.name);
    });
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_profiles.profiles().size())
            loadEditor(m_profiles.profiles().at(row));
    });
    connect(fromCurrent, &QPushButton::clicked, this, [this] {
        m_list->clearSelection();
        ConnectionProfile p = m_context.current;
        p.id.clear();
        loadEditor(p);
        m_name->setFocus(Qt::OtherFocusReason);
        m_name->selectAll();
    });
    connect(blank, &QPushButton::clicked, this, [this] {
        m_list->clearSelection();
        loadEditor(ConnectionProfile{});
        m_name->setFocus(Qt::OtherFocusReason);
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        if (m_editingId.isEmpty() || !m_profiles.remove(m_editingId))
            return;
        rebuildList();
        if (m_profiles.isEmpty())
            loadEditor(ConnectionProfile{});
        emit profilesChanged(m_profiles);
        setStatus(tr("Profile deleted."));
    });
    connect(save, &QPushButton::clicked, this, [this] {
        const ConnectionProfile p = editorProfile();
        if (p.name.trimmed().isEmpty()) {
            setStatus(tr("Give the profile a name."), true);
            return;
        }
        if (!p.isReachable()) {
            setStatus(p.route == ConnectionProfile::Route::SmartLink
                          ? tr("Choose the radio from the Radio list (log in to SmartLink first).")
                          : tr("Choose a radio from the list, or enter its address."),
                      true);
            return;
        }
        m_editingId = m_profiles.upsert(p);
        rebuildList(m_editingId);
        emit profilesChanged(m_profiles);
        setStatus(tr("Saved \"%1\".").arg(p.name.trimmed()));
    });
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    rebuildList();
    if (m_profiles.isEmpty()) {
        ConnectionProfile seed = m_context.current;
        seed.id.clear();
        loadEditor(seed);
    } else {
        m_list->setCurrentRow(0);
    }
}

void ConnectionProfilesDialog::rebuildList(const QString& selectId)
{
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    int select = -1;
    for (int i = 0; i < m_profiles.profiles().size(); ++i) {
        const ConnectionProfile& p = m_profiles.profiles().at(i);
        m_list->addItem(QStringLiteral("%1 — %2").arg(p.name, routeLabel(p.route)));
        if (p.id == selectId)
            select = i;
    }
    if (select >= 0)
        m_list->setCurrentRow(select);
    m_delete->setEnabled(!m_editingId.isEmpty() && m_profiles.find(m_editingId));
}

void ConnectionProfilesDialog::loadEditor(const ConnectionProfile& p)
{
    m_editingId = p.id;
    m_pickedRadioLabel.clear();
    m_name->setText(p.name);
    {
        const QSignalBlocker blocker(m_family);
        m_family->setCurrentIndex(std::max(0, m_family->findData(p.family)));
    }
    rebuildRoutes(p.route);
    m_serial->setText(p.serial);
    m_address->setText(p.address);
    m_flexGlobal->setCurrentText(p.flexGlobalProfile);
    m_flexTx->setCurrentText(p.flexTxProfile);
    m_frequency->setValue(p.frequencyMhz);
    m_mode->setCurrentText(p.mode);
    m_power->setValue(p.rfPower);
    m_delete->setEnabled(!p.id.isEmpty() && m_profiles.find(p.id));
    setStatus(QString());
}

ConnectionProfile ConnectionProfilesDialog::editorProfile() const
{
    ConnectionProfile p;
    if (const ConnectionProfile* existing = m_profiles.find(m_editingId))
        p = *existing;
    const QString previousFamily = p.family;
    p.id = m_editingId;
    p.name = m_name->text().trimmed();
    p.route = currentRoute();
    p.family = m_family->currentData().toString();
    p.serial = m_serial->text().trimmed();
    p.address = m_address->text().trimmed();
    p.flexGlobalProfile = m_flexGlobal->currentText().trimmed();
    p.flexTxProfile = m_flexTx->currentText().trimmed();
    p.frequencyMhz = m_frequency->value();
    p.mode = m_mode->currentText().trimmed();
    p.rfPower = m_power->value();

    // The label is line two of the tab, so it must name the radio this profile
    // reaches, not whichever radio was live when the editor opened.
    const ConnectionProfile& cur = m_context.current;
    const bool isCurrentRadio = p.family.compare(cur.family, Qt::CaseInsensitive) == 0
        && ((!p.serial.isEmpty() && p.serial == cur.serial)
            || (!p.address.isEmpty() && p.address == cur.address));
    if (!m_pickedRadioLabel.isEmpty())
        p.radioLabel = m_pickedRadioLabel;
    else if (isCurrentRadio && !cur.radioLabel.isEmpty())
        p.radioLabel = cur.radioLabel;
    else if (p.radioLabel.isEmpty() || previousFamily.compare(p.family, Qt::CaseInsensitive) != 0
             // Repairs a non-Flex profile saved with the live Flex's label.
             || (p.family != QLatin1String("flex")
                 && p.radioLabel.startsWith(QLatin1String("FLEX"), Qt::CaseInsensitive)))
        p.radioLabel = m_family->currentText();
    return p;
}

ConnectionProfile::Route ConnectionProfilesDialog::currentRoute() const
{
    return connectionRouteFromToken(m_route->currentData().toString());
}

void ConnectionProfilesDialog::rebuildRoutes(std::optional<ConnectionProfile::Route> prefer)
{
    const auto keep = prefer.value_or(currentRoute());
    const auto routes = connectionRoutesForFamily(m_family->currentData().toString());
    {
        const QSignalBlocker blocker(m_route);
        m_route->clear();
        for (auto r : routes)
            m_route->addItem(routeLabel(r), connectionRouteToken(r));
        m_route->setCurrentIndex(std::max(0, m_route->findData(connectionRouteToken(keep))));
    }
    m_route->setEnabled(routes.size() > 1);
    updateRouteFields();
}

void ConnectionProfilesDialog::updateRouteFields()
{
    const auto route = currentRoute();
    m_form->setRowVisible(m_serial, route != ConnectionProfile::Route::Ip);
    m_form->setRowVisible(m_address, route != ConnectionProfile::Route::SmartLink);

    // Offer only radios of this type that this route can reach: SmartLink
    // rows for SmartLink, discovered rows for LAN, any addressed radio for IP.
    const QString family = m_family->currentData().toString();
    const QSignalBlocker blocker(m_radioPick);
    m_radioPick->clear();
    m_radioPick->addItem(QString(), -1);
    for (int i = 0; i < m_context.knownRadios.size(); ++i) {
        const ConnectionProfile& r = m_context.knownRadios.at(i);
        if (r.family.compare(family, Qt::CaseInsensitive) != 0)
            continue;
        const bool fits = route == ConnectionProfile::Route::SmartLink
            ? r.route == ConnectionProfile::Route::SmartLink
            : route == ConnectionProfile::Route::Lan
                ? r.route == ConnectionProfile::Route::Lan
                : r.route != ConnectionProfile::Route::SmartLink && !r.address.isEmpty();
        if (!fits)
            continue;
        const QString where = r.route == ConnectionProfile::Route::SmartLink
            ? tr("SmartLink") : (r.address.isEmpty() ? tr("LAN") : r.address);
        m_radioPick->addItem(QStringLiteral("%1 — %2").arg(r.name, where), i);
    }
    m_radioPick->setItemText(0, m_radioPick->count() > 1 ? tr("Choose a radio…")
        : route == ConnectionProfile::Route::SmartLink ? tr("None — log in to SmartLink first")
        : tr("None found — enter the address below"));
    m_radioPick->setCurrentIndex(0);
}

void ConnectionProfilesDialog::setStatus(const QString& text, bool error)
{
    m_status->setText(text);
    m_status->setStyleSheet(error
        ? QStringLiteral("color: %1;").arg(ThemeManager::instance()
              .color(this, QStringLiteral("color.accent.danger")).name())
        : QString());
    m_status->setVisible(!text.isEmpty());
}

} // namespace AetherSDR
