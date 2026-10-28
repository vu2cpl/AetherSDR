#include "ConnectionProfiles.h"

#include <QJsonArray>
#include <QUuid>

#include <algorithm>

namespace AetherSDR {

QString connectionRouteToken(ConnectionProfile::Route route)
{
    switch (route) {
    case ConnectionProfile::Route::Ip:        return QStringLiteral("ip");
    case ConnectionProfile::Route::SmartLink: return QStringLiteral("smartlink");
    case ConnectionProfile::Route::Lan:       break;
    }
    return QStringLiteral("lan");
}

QList<ConnectionProfile::Route> connectionRoutesForFamily(const QString& family)
{
    using Route = ConnectionProfile::Route;
    const QString f = family.trimmed().toLower();
    if (f == QLatin1String("flex") || f.isEmpty())
        return {Route::Lan, Route::Ip, Route::SmartLink};
    if (f == QLatin1String("icom"))
        return {Route::Ip};
    return {Route::Lan, Route::Ip};
}

ConnectionProfile::Route connectionRouteFromToken(const QString& token)
{
    const QString t = token.trimmed().toLower();
    if (t == QLatin1String("ip"))
        return ConnectionProfile::Route::Ip;
    if (t == QLatin1String("smartlink"))
        return ConnectionProfile::Route::SmartLink;
    return ConnectionProfile::Route::Lan;
}

bool ConnectionProfile::isReachable() const
{
    switch (route) {
    case Route::Ip:        return !address.isEmpty();
    case Route::SmartLink: return !serial.isEmpty();
    case Route::Lan:       break;
    }
    // LAN prefers the discovered serial and falls back to the address.
    return !serial.isEmpty() || !address.isEmpty();
}

bool ConnectionProfile::appliesSettings() const
{
    return !flexGlobalProfile.isEmpty() || !flexTxProfile.isEmpty()
        || frequencyMhz > 0.0 || !mode.isEmpty() || rfPower >= 0;
}

QJsonObject ConnectionProfile::toJson() const
{
    QJsonObject obj{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("route"), connectionRouteToken(route)},
        {QStringLiteral("family"), family},
    };
    if (!serial.isEmpty())     obj[QStringLiteral("serial")] = serial;
    if (!address.isEmpty())    obj[QStringLiteral("address")] = address;
    if (!radioLabel.isEmpty()) obj[QStringLiteral("radioLabel")] = radioLabel;

    QJsonObject apply;
    if (!flexGlobalProfile.isEmpty()) apply[QStringLiteral("flexGlobalProfile")] = flexGlobalProfile;
    if (!flexTxProfile.isEmpty())     apply[QStringLiteral("flexTxProfile")] = flexTxProfile;
    if (frequencyMhz > 0.0)           apply[QStringLiteral("frequencyMhz")] = frequencyMhz;
    if (!mode.isEmpty())              apply[QStringLiteral("mode")] = mode;
    if (rfPower >= 0)                 apply[QStringLiteral("rfPower")] = rfPower;
    if (!apply.isEmpty())
        obj[QStringLiteral("apply")] = apply;
    return obj;
}

ConnectionProfile ConnectionProfile::fromJson(const QJsonObject& obj)
{
    ConnectionProfile p;
    p.id = obj.value(QStringLiteral("id")).toString();
    p.name = obj.value(QStringLiteral("name")).toString();
    p.route = connectionRouteFromToken(obj.value(QStringLiteral("route")).toString());
    p.family = obj.value(QStringLiteral("family")).toString(QStringLiteral("flex")).toLower();
    if (p.family.isEmpty())
        p.family = QStringLiteral("flex");
    p.serial = obj.value(QStringLiteral("serial")).toString();
    p.address = obj.value(QStringLiteral("address")).toString();
    p.radioLabel = obj.value(QStringLiteral("radioLabel")).toString();

    const QJsonObject apply = obj.value(QStringLiteral("apply")).toObject();
    p.flexGlobalProfile = apply.value(QStringLiteral("flexGlobalProfile")).toString();
    p.flexTxProfile = apply.value(QStringLiteral("flexTxProfile")).toString();
    p.frequencyMhz = std::max(0.0, apply.value(QStringLiteral("frequencyMhz")).toDouble());
    p.mode = apply.value(QStringLiteral("mode")).toString();
    const int power = apply.value(QStringLiteral("rfPower")).toInt(-1);
    p.rfPower = power < 0 ? -1 : std::min(power, 100);
    return p;
}

bool profileMatchesSession(const ConnectionProfile& profile,
                           const ConnectionSessionIdentity& session)
{
    if (!session.connected)
        return false;
    // A SmartLink profile and a LAN profile for the same radio are different
    // profiles; only the one whose route carried this session lights up.
    if ((profile.route == ConnectionProfile::Route::SmartLink) != session.wan)
        return false;
    if (profile.family.compare(session.family, Qt::CaseInsensitive) != 0)
        return false;
    if (!profile.serial.isEmpty() && !session.serial.isEmpty())
        return profile.serial.compare(session.serial, Qt::CaseInsensitive) == 0;
    if (!profile.address.isEmpty() && !session.address.isEmpty())
        return profile.address.compare(session.address, Qt::CaseInsensitive) == 0;
    return false;
}

ProfileClickAction decideProfileClick(bool connected, const QString& activeProfileId,
                                      const QString& clickedProfileId)
{
    if (!connected)
        return ProfileClickAction::Connect;
    if (!activeProfileId.isEmpty() && activeProfileId == clickedProfileId)
        return ProfileClickAction::ConfirmDisconnect;
    return ProfileClickAction::ConfirmSwitch;
}

const ConnectionProfile* ConnectionProfileSet::find(const QString& id) const
{
    for (const ConnectionProfile& p : m_profiles) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

QString ConnectionProfileSet::upsert(ConnectionProfile profile)
{
    if (profile.id.isEmpty())
        profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    for (ConnectionProfile& p : m_profiles) {
        if (p.id == profile.id) {
            p = profile;
            return profile.id;
        }
    }
    m_profiles.append(profile);
    return profile.id;
}

bool ConnectionProfileSet::remove(const QString& id)
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == id) {
            m_profiles.removeAt(i);
            return true;
        }
    }
    return false;
}

const ConnectionProfile* ConnectionProfileSet::matchSession(
    const ConnectionSessionIdentity& session) const
{
    for (const ConnectionProfile& p : m_profiles) {
        if (profileMatchesSession(p, session))
            return &p;
    }
    return nullptr;
}

QJsonObject ConnectionProfileSet::toJson() const
{
    QJsonArray list;
    for (const ConnectionProfile& p : m_profiles)
        list.append(p.toJson());
    return QJsonObject{
        {QStringLiteral("schemaVersion"), kSchemaVersion},
        {QStringLiteral("profiles"), list},
    };
}

ConnectionProfileSet ConnectionProfileSet::fromJson(const QJsonObject& obj)
{
    ConnectionProfileSet set;
    for (const QJsonValue& v : obj.value(QStringLiteral("profiles")).toArray()) {
        ConnectionProfile p = ConnectionProfile::fromJson(v.toObject());
        // A profile with no way to reach a radio, or a duplicate id, is
        // dropped rather than shown as a tab that cannot connect.
        const bool reachable = p.isReachable();
        if (p.id.isEmpty() || !reachable || set.find(p.id))
            continue;
        if (p.name.isEmpty())
            p.name = p.radioLabel.isEmpty() ? QStringLiteral("Profile") : p.radioLabel;
        set.m_profiles.append(p);
    }
    return set;
}

} // namespace AetherSDR
