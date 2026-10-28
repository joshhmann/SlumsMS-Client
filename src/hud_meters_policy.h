#pragma once
// ============================================================
// hud_meters_policy.h -- the EXP tracker and DPS meter maths (ticket 118). SCAFFOLD.
//
// No Windows, no client: tests/hud_meters_policy_test.cpp checks it. The HUD modules (next to HP/MP/EXP in
// overlay.cpp) will feed it: ExpTracker from MapleRoot_CurrentExp()/NextLevelExp() and the level each frame;
// DpsMeter from a CMob::OnHit (0x00668B83) detour when the attacker id is the local character (a1 vs
// CUserLocal+0x11A8, a2 = skill, a5 = damage). Research note: docs/research-118-122.md.
// ============================================================

#include <algorithm>
#include <cstdint>
#include <deque>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace HudMeters {

// EXP per hour over a rolling window. Gains carry across level-ups (the level's remaining EXP plus the new level's).
class ExpTracker {
public:
    explicit ExpTracker(uint32_t windowMs = 15 * 60 * 1000) : m_window(windowMs) {}

    void Sample(uint32_t nowMs, int level, int64_t exp, int64_t nextLevelExp) {
        if (m_haveLast) {
            int64_t gained = 0;
            if (level == m_level) gained = exp - m_exp;
            else if (level == m_level + 1) gained = (m_next - m_exp) + exp;   // levelled up in between
            if (gained > 0) {
                m_gains.emplace_back(nowMs, gained);
                m_session += gained;
            }
        } else {
            m_start = nowMs;
        }
        m_haveLast = true;
        m_level = level; m_exp = exp; m_next = nextLevelExp; m_now = nowMs;
        while (!m_gains.empty() && nowMs - m_gains.front().first > m_window) m_gains.pop_front();
    }

    // EXP per hour over min(window, time tracked); 0 until a minute has passed.
    int64_t PerHour() const {
        const uint32_t span = (m_now - m_start) < m_window ? (m_now - m_start) : m_window;
        if (span < 60 * 1000) return 0;
        int64_t sum = 0;
        for (const auto& g : m_gains) sum += g.second;
        return sum * 3600000 / span;
    }

    // Seconds to the next level at the current rate; -1 when unknown.
    int64_t SecondsToLevel() const {
        const int64_t rate = PerHour();
        if (rate <= 0 || m_next <= m_exp) return -1;
        return (m_next - m_exp) * 3600 / rate;
    }

    int64_t Session() const { return m_session; }

private:
    uint32_t m_window, m_start = 0, m_now = 0;
    bool m_haveLast = false;
    int m_level = 0;
    int64_t m_exp = 0, m_next = 0, m_session = 0;
    std::deque<std::pair<uint32_t, int64_t>> m_gains;
};

// My damage: DPS over a rolling window, the session total, and per-skill totals.
class DpsMeter {
public:
    explicit DpsMeter(uint32_t windowMs = 10 * 1000) : m_window(windowMs) {}

    void Hit(uint32_t nowMs, int skillId, int64_t damage) {
        if (damage <= 0) return;   // misses and "no number" hits
        m_hits.emplace_back(nowMs, damage);
        m_total += damage;
        m_bySkill[skillId] += damage;
        Trim(nowMs);
    }

    int64_t Dps(uint32_t nowMs) {
        Trim(nowMs);
        int64_t sum = 0;
        for (const auto& h : m_hits) sum += h.second;
        return sum * 1000 / m_window;
    }

    int64_t Total() const { return m_total; }

    // The n skills with the most damage, biggest first.
    std::vector<std::pair<int, int64_t>> Top(size_t n) const {
        std::vector<std::pair<int, int64_t>> v(m_bySkill.begin(), m_bySkill.end());
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
        if (v.size() > n) v.resize(n);
        return v;
    }

    void Reset() { m_hits.clear(); m_bySkill.clear(); m_total = 0; }

private:
    void Trim(uint32_t nowMs) {
        while (!m_hits.empty() && nowMs - m_hits.front().first > m_window) m_hits.pop_front();
    }
    uint32_t m_window;
    int64_t m_total = 0;
    std::deque<std::pair<uint32_t, int64_t>> m_hits;
    std::map<int, int64_t> m_bySkill;
};

// Party damage ranks from the server (LP 0x3739, BOSS_DAMAGE_RANK on the server's 161-dmgrank branch). After the
// opcode: int mobId; byte n; n x {int charId; short len + ASCII name; long damage}, biggest first, at most 6 rows.
// Sent about once a second per boss (or mob with 5M+ HP) to party members on the map who hit it; solo players get none.
inline constexpr unsigned short kDamageRankOpcode = 0x3739;
inline constexpr size_t kDamageRankMaxRows = 6;
inline constexpr size_t kDamageRankMaxName = 32;   // character names are 12; anything longer is a bad packet

struct RankRow {
    int charId = 0;
    std::string name;
    int64_t damage = 0;
};

struct DamageRank {
    int mobId = 0;
    std::vector<RankRow> rows;
};

// Reads the body after the opcode. False (and `out` untouched) when it is short, too long or out of bounds.
inline bool ParseDamageRank(const uint8_t* p, size_t len, DamageRank& out) {
    size_t at = 0;
    auto take = [&](void* dst, size_t n) {
        if (n > len - at) return false;
        memcpy(dst, p + at, n);
        at += n;
        return true;
    };
    DamageRank r;
    uint8_t n = 0;
    if (!p || !take(&r.mobId, 4) || !take(&n, 1) || n > kDamageRankMaxRows) return false;
    for (uint8_t i = 0; i < n; ++i) {
        RankRow row;
        uint16_t nameLen = 0;
        if (!take(&row.charId, 4) || !take(&nameLen, 2) || nameLen > kDamageRankMaxName) return false;
        row.name.resize(nameLen);
        if (nameLen && !take(&row.name[0], nameLen)) return false;
        if (!take(&row.damage, 8) || row.damage < 0) return false;
        r.rows.push_back(std::move(row));
    }
    out = std::move(r);
    return true;
}

// A row's share of the damage the rank lists, in whole percent (0 when the list sums to nothing).
inline int SharePct(const DamageRank& r, size_t row) {
    int64_t sum = 0;
    for (const auto& x : r.rows) sum += x.damage;
    if (sum <= 0 || row >= r.rows.size()) return 0;
    return static_cast<int>((r.rows[row].damage * 100 + sum / 2) / sum);
}

// The OnHit detour's entry bytes (mov eax, SEH handler; call __EH_prolog), checked before hooking.
inline constexpr uint32_t kAddr_CMob_OnHit = 0x00668B83;
inline constexpr uint8_t kOnHitStock[10] = {0xB8, 0x94, 0x17, 0xAA, 0x00, 0xE8, 0x0B, 0x80, 0x3F, 0x00};

}  // namespace HudMeters
