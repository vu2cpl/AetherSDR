#pragma once

#include "PersistentDialog.h"
#include "core/ConnectionProfiles.h"

#include <QStringList>

#include <optional>

class QFormLayout;

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace AetherSDR {

// Profiles → Connection Profiles…: create, edit and delete the profiles the
// title bar's radio strip shows. Edits are local until Save; every Save or
// Delete hands the whole set back through profilesChanged().
class ConnectionProfilesDialog : public PersistentDialog {
    Q_OBJECT

public:
    struct Context {
        // The live session as a ready-made profile (empty serial and address
        // when nothing is connected), for "New from current connection".
        ConnectionProfile current;
        // Radios reachable right now (LAN discovery, SmartLink list), each as
        // a ready-made profile, so the operator picks a radio rather than
        // typing its serial.
        QList<ConnectionProfile> knownRadios;
        QStringList flexGlobalProfiles;
        QStringList flexTxProfiles;
        QStringList modes;
    };

    ConnectionProfilesDialog(const ConnectionProfileSet& profiles, const Context& context,
                             QWidget* parent = nullptr);

signals:
    void profilesChanged(const ConnectionProfileSet& profiles);

private:
    void rebuildList(const QString& selectId = QString());
    void loadEditor(const ConnectionProfile& profile);
    ConnectionProfile editorProfile() const;
    ConnectionProfile::Route currentRoute() const;
    void rebuildRoutes(std::optional<ConnectionProfile::Route> prefer = std::nullopt);
    void updateRouteFields();
    void setStatus(const QString& text, bool error = false);

    ConnectionProfileSet m_profiles;
    Context m_context;
    QString m_editingId;
    QString m_pickedRadioLabel;   // from the Radio list, until the next load

    QListWidget*    m_list{nullptr};
    QFormLayout*    m_form{nullptr};
    QComboBox*      m_radioPick{nullptr};
    QLineEdit*      m_name{nullptr};
    QComboBox*      m_route{nullptr};
    QComboBox*      m_family{nullptr};
    QLineEdit*      m_serial{nullptr};
    QLineEdit*      m_address{nullptr};
    QComboBox*      m_flexGlobal{nullptr};
    QComboBox*      m_flexTx{nullptr};
    QDoubleSpinBox* m_frequency{nullptr};
    QComboBox*      m_mode{nullptr};
    QSpinBox*       m_power{nullptr};
    QPushButton*    m_delete{nullptr};
    QLabel*         m_status{nullptr};
};

} // namespace AetherSDR
