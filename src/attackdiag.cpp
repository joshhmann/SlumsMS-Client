// Bot attack diagnostic (ticket 32). Off unless config.ini has [debug] attackdiag=1 (drops only) or =2 (every
// remote attack): then the client writes attackdiag.log next to the exe with every other character's attack it
// drops or misdraws, and why - attacker id and name, skill, level, pose, the weapon - so a short AFK session next
// to bots (the server's !botshowcase) gives a full report. Nothing is changed in how attacks are drawn.
//
// Hooks (none of these addresses is hooked or patched elsewhere in slums.dll or the MapleRoot companion):
//   0x009803AB CUserRemote::OnAttack(int nType, CInPacket*)      - decodes the packet first, reports after
//   0x0092EDB2 CUser::CheckAttackAction(nAction, nSpeed, pSkill, nSLV) - its verdict (0 = the whole attack dropped)
//   0x00980A81 CUserRemote::OnPrepare(CInPacket*)                - a keydown packet for a skill the client lacks
#include "pch.h"
#include "hook.h"
#include "constants.h"
#include "wvs/packet.h"
#include "attackdiag_rules.h"
#include <cstdio>
#include <ctime>

namespace {

auto CUserRemote__OnAttack = reinterpret_cast<void(__thiscall*)(void*, int, CInPacket*)>(0x009803AB);
auto CUser__CheckAttackAction = reinterpret_cast<int(__thiscall*)(void*, int, int, void*, int)>(0x0092EDB2);
auto CUserRemote__OnPrepare = reinterpret_cast<void(__thiscall*)(void*, CInPacket*)>(0x00980A81);

auto CSkillInfo__GetSkill = reinterpret_cast<void*(__thiscall*)(void*, int)>(0x0075C755);
auto SKILLENTRY__IsCorrectWeaponType = reinterpret_cast<int(__thiscall*)(void*, int)>(0x00766157);
auto get_weapon_type = reinterpret_cast<int(__cdecl*)(int)>(0x00460AA0);
auto is_shoot_skill_without_weapon = reinterpret_cast<int(__cdecl*)(int)>(0x0076681D);
void** const ppSkillInfo = reinterpret_cast<void**>(0x00BE78DC);

// CUser fields (CAvatar at +0x88): the weapon item, its Character.wz attack type and afterImage, the name; the
// CUserRemote keydown state OnPrepare sets.
constexpr size_t kOffWeaponItem = 0x4EC, kOffAttackType = 0x4F8, kOffAfterImage = 0x508, kOffName = 0x11AC;
constexpr size_t kOffKeydownOn = 0x37CC, kOffKeydownSkill = 0x37C0;

const char* const kActions[attackdiag::kActionCount] = {
    "walk1", "walk2", "stand1", "stand2", "alert", "swingO1", "swingO2", "swingO3", "swingOF", "swingT1",
    "swingT2", "swingT3", "swingTF", "swingP1", "swingP2", "swingPF", "stabO1", "stabO2", "stabOF", "stabT1",
    "stabT2", "stabTF", "shoot1", "shoot2", "swingO1", "swingO2", "swingO3", "shootF", "swingO1", "swingO3",
    "stabO1", "heal", "proneStab", "prone", "fly", "jump", "ladder", "rope", "dead", "sit",
    "tired", "alert2", "alert3", "alert4", "alert5", "alert6", "ladder2", "rope2", "shoot6", "magic1",
    "magic2", "magic3", "magic5", "burster1", "burster2", "savage", "avenger", "assaulter", "prone2", "assassination",
    "assassinationS", "rush", "rush2", "brandish1", "brandish2", "sanctuary", "meteor", "paralyze", "blizzard", "genesis",
    "ninjastorm", "blast", "holyshield", "showdown", "resurrection", "chainlightning", "smokeshell", "handgun", "somersault", "straight",
    "eburster", "backspin", "eorb", "screw", "doubleupper", "dragonstrike", "doublefire", "triplefire", "fake", "airstrike",
    "edrain", "octopus", "backstep", "shot", "recovery", "fireburner", "coolingeffect", "fist", "timeleap", "rapidfire",
    "homing", "ghostwalk", "ghoststand", "ghostjump", "ghostproneStab", "ghostfly", "ghostladder", "ghostrope", "ghostsit", "cannon",
    "torpedo", "darksight", "bamboo", "pyramid", "wave", "blade", "souldriver", "firestrike", "flamegear", "stormbreak",
    "vampire", "float", "swingT2PoleArm", "swingP1PoleArm", "swingP2PoleArm", "doubleSwing", "tripleSwing", "fullSwingDouble", "fullSwingTriple", "overSwingDouble",
    "overSwingTriple", "rollingSpin", "comboSmash", "comboFenrir", "comboTempest", "finalCharge", "combatStep", "finalBlow", "finalToss", "magicmissile",
    "dragonSpark", "dragonBreathe", "breathe_prepare", "dragonIceBreathe", "icebreathe_prepare", "infinityExplosion", "superMagicmissile", "illusion", "magicFlare", "elementalReset",
    "elementalRegistance", "dragonAura", "magicBooster", "dragonShield", "dragonFury", "dragonFly", "dragonSkin", "shockwave", "demolition", "snatch",
    "windspear", "windshot"};

int g_mode = 0; // 1: drops, 2: every remote attack
FILE* g_log = nullptr;

struct Pending {
    void* user = nullptr;
    int checkResult = -1;
} g_pending;

template <typename T>
T Field(void* base, size_t off) {
    return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(base) + off);
}

// The ZXString<char> data at a field, copied defensively (a bad pointer gives "?").
void CopyString(void* base, size_t off, char* out, size_t n) {
    out[0] = 0;
    __try {
        const char* s = Field<const char*>(base, off);
        if (!s) return;
        size_t i = 0;
        for (; i + 1 < n && s[i]; ++i) out[i] = s[i];
        out[i] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(out, n, "?");
    }
}

void* GetSkill(int skillId) {
    void* info = *ppSkillInfo;
    return info && skillId ? CSkillInfo__GetSkill(info, skillId) : nullptr;
}

void Write(const char* line) {
    if (!g_log) return;
    time_t now = time(nullptr);
    tm t{};
    localtime_s(&t, &now);
    fprintf(g_log, "%04d-%02d-%02d %02d:%02d:%02d %s\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
            t.tm_min, t.tm_sec, line);
    fflush(g_log);
}

const char* TypeName(int type) {
    switch (type) {
        case attackdiag::kCloseRange: return "melee";
        case attackdiag::kRanged: return "shot";
        case attackdiag::kMagic: return "spell";
        case attackdiag::kBody: return "body";
        default: return "?";
    }
}

bool KeepsKeydownPose(int s) { return s == 3121004 || s == 5221004 || s == 15111006 || s == 13111002; }
bool SkipsCheck(int s) {
    return s == 2301002 || s == 3121004 || s == 5221004 || s == 5201002 || s == 15111006 || s == 13111002 || s == 14111006;
}

void __fastcall OnAttack_hook(void* pThis, void* /*edx*/, int nType, CInPacket* iPacket) {
    attackdiag::Attack attack;
    int charId = 0;
    bool decoded = false;
    if (iPacket) {
        const unsigned char* buf = iPacket->BufferPublic();
        size_t off = iPacket->GetOffset(), len = iPacket->LengthPublic();
        if (buf && off <= len) {
            if (off >= 4) memcpy(&charId, buf + off - 4, 4);
            attack = attackdiag::Decode(nType, buf + off, len - off);
            decoded = true;
        }
    }
    if (decoded && (attack.truncated || attack.action >= attackdiag::kActionCount)) {
        // the client may throw or read past its tables inside OnAttack: write the line first
        char early[256];
        sprintf_s(early, "DROP char %d %s skill %d pose %d | %s", charId, TypeName(nType), attack.skillId,
                  attack.action, attack.truncated ? "packet shorter than OnAttack reads (CInPacket throws)"
                                                  : "pose outside the action table (0x009807CF reads past it)");
        Write(early);
    }
    g_pending = Pending{pThis, -1};
    CUserRemote__OnAttack(pThis, nType, iPacket);
    int checkResult = g_pending.checkResult;
    g_pending = Pending{};
    if (!decoded) return;

    attackdiag::Facts f;
    void* skill = GetSkill(attack.skillId);
    int weaponId = Field<int>(pThis, kOffWeaponItem);
    f.skillKnown = attack.skillId == 0 || skill != nullptr;
    f.weaponType = get_weapon_type(weaponId);
    f.attackType = Field<int>(pThis, kOffAttackType);
    char afterImage[32];
    CopyString(pThis, kOffAfterImage, afterImage, sizeof(afterImage));
    f.hasAfterImage = afterImage[0] != 0;
    f.shootsWithoutWeapon = attack.skillId && is_shoot_skill_without_weapon(attack.skillId);
    f.keepsKeydownPose = KeepsKeydownPose(attack.skillId);
    f.inKeydown = Field<int>(pThis, kOffKeydownOn) == 1 && Field<int>(pThis, kOffKeydownSkill) == attack.skillId;
    f.checkSkipped = SkipsCheck(attack.skillId);
    f.checkResult = checkResult;
    f.weaponFitsSkill = !skill || SKILLENTRY__IsCorrectWeaponType(skill, f.weaponType);
    std::vector<std::string> reasons = attackdiag::Reasons(attack, f);
    if (reasons.empty() && g_mode < 2) return;

    char name[32];
    CopyString(pThis, kOffName, name, sizeof(name));
    char line[1024];
    int n = sprintf_s(line, "%s char %d '%s' %s skill %d lv %d pose %d(%s) %s speed %d weapon %d(type %d, attack %d%s)",
        reasons.empty() ? "ok  " : "DROP", charId, name, TypeName(attack.type), attack.skillId, attack.skillLevel,
        attack.action, attack.action < attackdiag::kActionCount ? kActions[attack.action] : "?",
        attack.left ? "L" : "R", attack.speed, weaponId, f.weaponType, f.attackType,
        f.hasAfterImage ? "" : ", no afterImage");
    for (const std::string& r : reasons) {
        if (n > 0 && n < static_cast<int>(sizeof(line)) - 4) {
            n += sprintf_s(line + n, sizeof(line) - n, " | %s", r.c_str());
        }
    }
    Write(line);
}

int __fastcall CheckAttackAction_hook(void* pThis, void* /*edx*/, int nAction, int nSpeed, void* pSkill, int nSLV) {
    int result = CUser__CheckAttackAction(pThis, nAction, nSpeed, pSkill, nSLV);
    if (pThis == g_pending.user) {
        g_pending.checkResult = result;
    }
    return result;
}

void __fastcall OnPrepare_hook(void* pThis, void* /*edx*/, CInPacket* iPacket) {
    if (iPacket) {
        const unsigned char* buf = iPacket->BufferPublic();
        size_t off = iPacket->GetOffset(), len = iPacket->LengthPublic();
        if (buf && off + 8 <= len) {
            int skillId = 0, charId = 0, pose = 0;
            memcpy(&skillId, buf + off, 4);
            if (off >= 4) memcpy(&charId, buf + off - 4, 4);
            pose = (buf[off + 5] | (buf[off + 6] << 8)) & 0x7FFF;
            char name[32];
            CopyString(pThis, kOffName, name, sizeof(name));
            char line[256];
            if (!GetSkill(skillId)) {
                sprintf_s(line, "DROP char %d '%s' keydown skill %d | skill not in this client's Skill.wz: OnPrepare throws",
                          charId, name, skillId);
                Write(line);
            } else if (pose != 0x7FFF && pose >= attackdiag::kActionCount) {
                sprintf_s(line, "DROP char %d '%s' keydown skill %d pose %d | pose outside the action table", charId,
                          name, skillId, pose);
                Write(line);
            } else if (g_mode >= 2) {
                sprintf_s(line, "ok   char %d '%s' keydown skill %d pose %d", charId, name, skillId, pose);
                Write(line);
            }
        }
    }
    CUserRemote__OnPrepare(pThis, iPacket);
}

} // namespace

void AttachAttackDiagMod() {
    g_mode = GetPrivateProfileIntA("debug", "attackdiag", 0, ".\\" CONSTANTS_CONFIG_NAME);
    if (g_mode <= 0) {
        return; // off: nothing hooked
    }
    if (fopen_s(&g_log, "attackdiag.log", "a") != 0) {
        g_log = nullptr;
        return;
    }
    Write(g_mode >= 2 ? "attack diagnostic on: every remote attack" : "attack diagnostic on: dropped remote attacks");
    ATTACH_HOOK(CUserRemote__OnAttack, OnAttack_hook);
    ATTACH_HOOK(CUser__CheckAttackAction, CheckAttackAction_hook);
    ATTACH_HOOK(CUserRemote__OnPrepare, OnPrepare_hook);
}
