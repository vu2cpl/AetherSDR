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

    m_radioPick = new QComboBox(bodyWidget());
    m_radioPick->setObjectName(QStringLiteral("connectionProfileRadio"));
    m_radioPick->setAccessibleName(tr("Radio"));
    m_radioPick->addItem(m_context.knownRadios.isEmpty()
                             ? tr("No radios found — fill in below")
                             : tr("Choose a radio…"));
    for (const ConnectionProfile& r : std::as_const(m_context.knownRadios)) {
        const QString where = r.route == ConnectionProfile::Route::SmartLink
            ? tr("SmartLink") : (r.address.isEmpty() ? tr("LAN") : r.address);
        m_radioPick->addItem(QStringLiteral("%1 — %2").arg(r.name, where));
    }
    form->addRow(tr("Radio"), m_radioPick);

    m_route = new QComboBox(bodyWidget());
    for (auto r : {ConnectionProfile::Route::Lan, ConnectionProfile::Route::Ip,
                   ConnectionProfile::Route::SmartLink})
        m_route->addItem(routeLabel(r), connectionRouteToken(r));
    form->addRow(tr("Connect via"), m_route);

    m_family = new QComboBox(bodyWidget());
    m_family->addItem(QStringLiteral("FlexRadio"), QStringLiteral("flex"));
    m_family->addItem(QStringLiteral("Icom"), QStringLiteral("icom"));
    m_family->addItem(QStringLiteral("Hermes-Lite 2"), QStringLiteral("hl2"));
    m_family->addItem(QStringLiteral("ANAN"), QStringLiteral("anan"));
    form->addRow(tr("Radio type"), m_family);

    m_serial = new QLineEdit(bodyWidget());
    m_serial->setPlaceholderText(tr("filled in when you choose a radio"));
    form->addRow(tr("Serial"), m_serial);
    m_address = new QLineEdit(bodyWidget());
    m_address->setPlaceholderText(QStringLiteral("192.168.1.148"));
    form->addRow(tr("Address"), m_address);

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

    connect(m_route, &QComboBox::currentIndexChanged, this, &ConnectionProfilesDialog::updateRouteFields);
    connect(m_radioPick, &QComboBox::activated, this, [this](int index) {
        if (index <= 0 || index > m_context.knownRadios.size())
            return;
        const ConnectionProfile& r = m_context.knownRadios.at(index - 1);
        m_route->setCurrentIndex(std::max(0, m_route->findData(connectionRouteToken(r.route))));
        m_family->setCurrentIndex(std::max(0, m_family->findData(r.family)));
        m_serial->setText(r.serial);
        m_address->setText(r.address);
        m_pickedRadioLabel = r.radioLabel;
        if (m_name->text().trimmed().isEmpty())
            m_name->setText(r.name);
        updateRouteFields();
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
    m_radioPick->setCurrentIndex(0);
    m_name->setText(p.name);
    m_route->setCurrentIndex(std::max(0, m_route->findData(connectionRouteToken(p.route))));
    m_family->setCurrentIndex(std::max(0, m_family->findData(p.family)));
    m_serial->setText(p.serial);
    m_address->setText(p.address);
    m_flexGlobal->setCurrentText(p.flexGlobalProfile);
    m_flexTx->setCurrentText(p.flexTxProfile);
    m_frequency->setValue(p.frequencyMhz);
    m_mode->setCurrentText(p.mode);
    m_power->setValue(p.rfPower);
    m_delete->setEnabled(!p.id.isEmpty() && m_profiles.find(p.id));
    setStatus(QString());
    updateRouteFields();
}

ConnectionProfile ConnectionProfilesDialog::editorProfile() const
{
    ConnectionProfile p;
    if (const ConnectionProfile* existing = m_profiles.find(m_editingId))
        p = *existing;
    else if (m_context.current.serial == m_serial->text().trimmed())
        p.radioLabel = m_context.current.radioLabel;
    if (!m_pickedRadioLabel.isEmpty())
        p.radioLabel = m_pickedRadioLabel;
    p.id = m_editingId;
    p.name = m_name->text().trimmed();
    p.route = connectionRouteFromToken(m_route->currentData().toString());
    p.family = m_family->currentData().toString();
    p.serial = m_serial->text().trimmed();
    p.address = m_address->text().trimmed();
    p.flexGlobalProfile = m_flexGlobal->currentText().trimmed();
    p.flexTxProfile = m_flexTx->currentText().trimmed();
    p.frequencyMhz = m_frequency->value();
    p.mode = m_mode->currentText().trimmed();
    p.rfPower = m_power->value();
    if (p.radioLabel.isEmpty())
        p.radioLabel = m_family->currentText();
    return p;
}

void ConnectionProfilesDialog::updateRouteFields()
{
    const auto route = connectionRouteFromToken(m_route->currentData().toString());
    // SmartLink only reaches Flex radios.
    if (route == ConnectionProfile::Route::SmartLink)
        m_family->setCurrentIndex(std::max(0, m_family->findData(QStringLiteral("flex"))));
    m_family->setEnabled(route != ConnectionProfile::Route::SmartLink);
    m_serial->setEnabled(route != ConnectionProfile::Route::Ip);
    m_address->setEnabled(route != ConnectionProfile::Route::SmartLink);
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
