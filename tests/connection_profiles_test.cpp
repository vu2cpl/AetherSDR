// Connection profiles: JSON round trip, load-time validation, the session
// match that lights a profile tab, and the click decision. QtCore only.

#include "core/ConnectionProfiles.h"

#include <QJsonArray>
#include <cstdio>

using namespace AetherSDR;

namespace {
int failures = 0;
int checks = 0;
void check(bool pass, const char* name)
{
    ++checks;
    if (!pass) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
}

ConnectionProfile flexLan()
{
    ConnectionProfile p;
    p.name = QStringLiteral("MSHV");
    p.route = ConnectionProfile::Route::Lan;
    p.family = QStringLiteral("flex");
    p.serial = QStringLiteral("2621-1104-6600-0383");
    p.address = QStringLiteral("192.168.1.148");
    p.radioLabel = QStringLiteral("FLEX-6600");
    p.flexGlobalProfile = QStringLiteral("MSHV FT8");
    p.flexTxProfile = QStringLiteral("DAX");
    return p;
}

ConnectionProfile icomIp()
{
    ConnectionProfile p;
    p.name = QStringLiteral("IC-705 FT8");
    p.route = ConnectionProfile::Route::Ip;
    p.family = QStringLiteral("icom");
    p.address = QStringLiteral("192.168.30.154");
    p.frequencyMhz = 14.074;
    p.mode = QStringLiteral("USB-D");
    p.rfPower = 10;
    return p;
}

void roundTrip()
{
    ConnectionProfileSet set;
    const QString a = set.upsert(flexLan());
    ConnectionProfile wan = flexLan();
    wan.name = QStringLiteral("JTDX-VU");
    wan.route = ConnectionProfile::Route::SmartLink;
    wan.flexGlobalProfile.clear();
    const QString b = set.upsert(wan);
    const QString c = set.upsert(icomIp());
    check(!a.isEmpty() && a != b && b != c, "upsert assigns distinct ids");

    const ConnectionProfileSet back = ConnectionProfileSet::fromJson(set.toJson());
    check(back.profiles().size() == 3, "three profiles survive a round trip");
    check(back.toJson() == set.toJson(), "round trip is lossless");
    check(set.toJson().value(QStringLiteral("schemaVersion")).toInt()
              == ConnectionProfileSet::kSchemaVersion, "schema version is written");

    const ConnectionProfile* icom = back.find(c);
    check(icom && icom->route == ConnectionProfile::Route::Ip, "ip route survives");
    check(icom && qFuzzyCompare(icom->frequencyMhz, 14.074), "frequency survives");
    check(icom && icom->mode == QStringLiteral("USB-D") && icom->rfPower == 10,
          "mode and power survive");
    const ConnectionProfile* jtdx = back.find(b);
    check(jtdx && jtdx->flexGlobalProfile.isEmpty() && jtdx->flexTxProfile == QStringLiteral("DAX"),
          "unset apply fields stay unset");

    ConnectionProfile renamed = *back.find(a);
    renamed.name = QStringLiteral("MSHV 40m");
    ConnectionProfileSet edited = back;
    check(edited.upsert(renamed) == a && edited.profiles().size() == 3,
          "upsert with an id replaces in place");
    check(edited.find(a)->name == QStringLiteral("MSHV 40m"), "replacement is stored");
    check(edited.remove(b) && !edited.find(b) && !edited.remove(b), "remove is exact");
}

void loadValidation()
{
    QJsonArray list;
    QJsonObject noId = icomIp().toJson();
    noId.remove(QStringLiteral("id"));
    list.append(noId);
    ConnectionProfile unreachable = icomIp();
    unreachable.id = QStringLiteral("u");
    unreachable.address.clear();
    list.append(unreachable.toJson());
    ConnectionProfile good = flexLan();
    good.id = QStringLiteral("g");
    good.name.clear();
    list.append(good.toJson());
    list.append(good.toJson());   // duplicate id
    QJsonObject wild = icomIp().toJson();
    wild[QStringLiteral("id")] = QStringLiteral("w");
    wild[QStringLiteral("route")] = QStringLiteral("carrier-pigeon");
    wild[QStringLiteral("serial")] = QStringLiteral("S");
    QJsonObject apply = wild.value(QStringLiteral("apply")).toObject();
    apply[QStringLiteral("rfPower")] = 250;
    wild[QStringLiteral("apply")] = apply;
    list.append(wild);

    const ConnectionProfileSet set =
        ConnectionProfileSet::fromJson(QJsonObject{{QStringLiteral("profiles"), list}});
    check(set.profiles().size() == 2, "id-less, unreachable and duplicate profiles are dropped");
    check(set.find(QStringLiteral("g")) && set.find(QStringLiteral("g"))->name == QStringLiteral("FLEX-6600"),
          "a nameless profile is named after its radio");
    const ConnectionProfile* w = set.find(QStringLiteral("w"));
    check(w && w->route == ConnectionProfile::Route::Lan, "an unknown route reads as LAN");
    check(w && w->rfPower == 100, "power is clamped to 100");
    check(ConnectionProfileSet::fromJson(QJsonObject{}).isEmpty(), "missing key loads empty");
}

void sessionMatch()
{
    ConnectionProfileSet set;
    const QString lan = set.upsert(flexLan());
    ConnectionProfile wan = flexLan();
    wan.route = ConnectionProfile::Route::SmartLink;
    const QString smartlink = set.upsert(wan);
    const QString icom = set.upsert(icomIp());

    ConnectionSessionIdentity s;
    s.connected = true;
    s.family = QStringLiteral("flex");
    s.serial = QStringLiteral("2621-1104-6600-0383");
    check(set.matchSession(s) && set.matchSession(s)->id == lan, "LAN session lights the LAN profile");
    s.wan = true;
    check(set.matchSession(s) && set.matchSession(s)->id == smartlink,
          "SmartLink session lights the SmartLink profile, not the LAN one");

    ConnectionSessionIdentity i;
    i.connected = true;
    i.family = QStringLiteral("icom");
    i.address = QStringLiteral("192.168.30.154");
    check(set.matchSession(i) && set.matchSession(i)->id == icom, "Icom matches by address");
    i.family = QStringLiteral("flex");
    check(!set.matchSession(i), "same address, other family does not match");
    i.family = QStringLiteral("icom");
    i.connected = false;
    check(!set.matchSession(i), "nothing matches when disconnected");
}

void clickDecision()
{
    check(decideProfileClick(false, QString(), QStringLiteral("a")) == ProfileClickAction::Connect,
          "idle click connects");
    check(decideProfileClick(true, QStringLiteral("a"), QStringLiteral("a"))
              == ProfileClickAction::ConfirmDisconnect, "clicking the live profile asks to disconnect");
    check(decideProfileClick(true, QStringLiteral("a"), QStringLiteral("b"))
              == ProfileClickAction::ConfirmSwitch, "clicking another profile asks to switch");
    check(decideProfileClick(true, QString(), QStringLiteral("b"))
              == ProfileClickAction::ConfirmSwitch, "a non-profile session also asks before switching");
}
} // namespace

int main()
{
    roundTrip();
    loadValidation();
    sessionMatch();
    clickDecision();
    std::printf("connection profiles: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
