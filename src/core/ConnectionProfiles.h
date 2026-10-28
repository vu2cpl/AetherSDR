#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace AetherSDR {

// A named way to reach one radio, plus what to apply once connected.
//
// The title bar's radio strip shows these in place of discovered radios once
// at least one exists, so "MSHV on the 6600 over LAN" and "JTDX-VU on the 6600
// over SmartLink" are one click each. The whole set is one JSON object under
// the "ConnectionProfiles" AppSettings key (Principle V); this type is QtCore
// only so the encoding and the session match are unit-testable.
struct ConnectionProfile {
    enum class Route { Lan, Ip, SmartLink };

    QString id;        // stable, generated once; the tab id is "profile:" + id
    QString name;      // operator's label, e.g. "MSHV"
    Route   route{Route::Lan};
    QString family{QStringLiteral("flex")};
    QString serial;    // Lan and SmartLink select by serial
    QString address;   // Ip connects here; Lan falls back to it when undiscovered
    QString radioLabel;   // "FLEX-6600" — line two of the tab

    // Applied after the connection lands. Empty / negative means "leave alone".
    QString flexGlobalProfile;   // Flex only
    QString flexTxProfile;       // Flex only
    double  frequencyMhz{0.0};   // active slice
    QString mode;                // active slice, e.g. "DIGU", "USB-D"
    int     rfPower{-1};         // 0-100

    bool appliesSettings() const;

    QJsonObject toJson() const;
    static ConnectionProfile fromJson(const QJsonObject& obj);
};

QString connectionRouteToken(ConnectionProfile::Route route);
ConnectionProfile::Route connectionRouteFromToken(const QString& token);

// What identifies the session that is live right now, for matching a profile.
struct ConnectionSessionIdentity {
    bool    connected{false};
    bool    wan{false};
    QString family;
    QString serial;
    QString address;
};

bool profileMatchesSession(const ConnectionProfile& profile,
                           const ConnectionSessionIdentity& session);

// What a click on a profile tab should do. Every action that ends the live
// session asks first; only a click with nothing connected acts at once.
enum class ProfileClickAction {
    Connect,             // nothing connected
    ConfirmDisconnect,   // the clicked profile is the live one
    ConfirmSwitch        // another session is live
};

ProfileClickAction decideProfileClick(bool connected, const QString& activeProfileId,
                                      const QString& clickedProfileId);

class ConnectionProfileSet {
public:
    static constexpr int kSchemaVersion = 1;
    static constexpr const char* kSettingsKey = "ConnectionProfiles";

    const QList<ConnectionProfile>& profiles() const { return m_profiles; }
    bool isEmpty() const { return m_profiles.isEmpty(); }
    const ConnectionProfile* find(const QString& id) const;

    // Inserts, or replaces the profile with the same id. A profile without an
    // id gets a fresh one; the stored id is returned.
    QString upsert(ConnectionProfile profile);
    bool remove(const QString& id);

    // The first profile that describes the live session, or nullptr.
    const ConnectionProfile* matchSession(const ConnectionSessionIdentity& session) const;

    QJsonObject toJson() const;
    static ConnectionProfileSet fromJson(const QJsonObject& obj);

private:
    QList<ConnectionProfile> m_profiles;
};

} // namespace AetherSDR
