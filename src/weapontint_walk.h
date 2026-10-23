#pragma once
// ============================================================
// weapontint_walk.h: which WZ canvases a Coloring Prism tint swaps in for one avatar
// layer build, written against an abstract tree so it can be tested off the client.
//
// weapontint.cpp instantiates it over the live COM tree (IWzProperty / IWzCanvas);
// tests/weapontint_walk_test.cpp instantiates it over a fake tree that reproduces the
// shapes that broke in 1.5.0-rc4:
//
//   * A cash weapon has NO node for the climbing actions (`rope`, `ladder`) -- nor for
//     sit, swim, or any skill action the artist did not draw. The old walk took "no node
//     for this action" to mean "an emotion-keyed Accessory img" and dyed the WHOLE img:
//     every weapon type, every action, every frame, a few hundred canvases tinted pixel
//     by pixel through a COM DrawRectangle per run. That was the client-wide freeze on
//     grabbing a rope. The whole-img fallback is now only for Accessory imgs.
//
//   * Most cash weapons draw one weapon type and point the rest at it with UOLs
//     (01702200.img/31/stand1 = "../30/stand1", for 24 of its 25 types). Each UOL'd type
//     reached the SAME property node again, found the clones the first pass had just put
//     there, and cloned the clones: a chain of up to 24 re-tints per canvas, each one
//     compounding Chroma and Brightness, all cached forever. A canvas this module made is
//     now never tinted again inside a pass.
//
// Ops is the tree adapter:
//   using Node   = ...;   // a property node (copyable handle)
//   using Canvas = ...;   // a canvas (copyable handle)
//   std::vector<std::wstring> Children(const Node&);
//   Kind Get(const Node& parent, const std::wstring& name, Node* sub, Canvas* canvas);
//        -- Canvas FIRST: a WZ canvas also answers as a property (origin / z / map).
//   bool IsOurs(const Canvas&);        // a tinted clone this module made
//   bool Swap(const Node& parent, const std::wstring& name, const Canvas& original);
//        -- tint + put the clone in place + remember the original; false if no clone
// ============================================================
#include <cstddef>
#include <cstdint>
#include <string>
#include <map>
#include <utility>
#include <unordered_set>
#include <vector>

namespace tintwalk {

enum class Kind { None, Canvas, Property };
enum class Layer { Body, Effects };

inline wchar_t FoldW(wchar_t c) { return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c; }

// Effects live under a node whose NAME starts with "effect" (case-insensitive).
inline bool IsEffectPart(const wchar_t* name) {
    if (!name) return false;
    static const wchar_t kEffect[] = L"effect";
    for (int i = 0; i < 6; ++i) {
        if (FoldW(name[i]) != kEffect[i]) return false;   // also stops at a short name's NUL
    }
    return true;
}

inline bool IsWeaponTypeNode(const std::wstring& name) {
    return !name.empty() && name[0] >= L'0' && name[0] <= L'9';
}

// Whether an item whose img has no node for the action being built should have EVERY
// subtree dyed instead. Only emotion-keyed imgs (Character/Accessory: face, eye and
// earring accessories) are shaped that way. For anything with action nodes -- weapons,
// clothes, caps, capes, shields -- a missing action means the client draws nothing of
// that item for it, so there is nothing to tint, and walking the whole img is pure cost.
inline bool WalkWholeImgWhenUnmatched(const wchar_t* subdir) {
    if (!subdir) return false;
    static const wchar_t kAcc[] = L"accessory";
    for (int i = 0;; ++i) {
        if (FoldW(subdir[i]) != kAcc[i]) return false;
        if (kAcc[i] == 0) return true;
    }
}

struct Stats {
    int canvases = 0;     // canvases looked at
    int swapped = 0;      // clones put in place
    int skippedOurs = 0;  // canvases that were already our clone (UOL'd duplicates)
    bool matched = false; // the img had a node for the action
    bool wholeImg = false;
};

template <class Ops>
class Walker {
public:
    using Node = typename Ops::Node;
    using Canvas = typename Ops::Canvas;

    explicit Walker(Ops& ops) : ops_(ops) {}
    const Stats& stats() const { return st_; }

    // One worn item's art for the action being built. `img` is Character/<subdir>/<id>.img.
    void SwapItem(const Node& img, const std::wstring& action, Layer layer, bool wholeImgWhenUnmatched) {
        Node sub;
        Canvas cv;
        for (const std::wstring& n : { action, std::wstring(L"default") }) {
            if (n.empty()) continue;
            if (ops_.Get(img, n, &sub, &cv) == Kind::Property) { SwapLayerOf(sub, layer); st_.matched = true; }
        }
        // Cash weapons nest one level deeper, under the numeric code of the weapon type the
        // sticker is worn over (01702000.img/33/stand1/0/weapon).
        for (const std::wstring& name : ops_.Children(img)) {
            if (!IsWeaponTypeNode(name)) continue;
            Node type;
            if (ops_.Get(img, name, &type, &cv) != Kind::Property) continue;
            for (const std::wstring& n : { action, std::wstring(L"default") }) {
                if (n.empty()) continue;
                if (ops_.Get(type, n, &sub, &cv) == Kind::Property) { SwapLayerOf(sub, layer); st_.matched = true; }
            }
        }
        if (st_.matched || !wholeImgWhenUnmatched) return;
        // An emotion-keyed img (Accessory): one canvas per emotion, so all of them.
        st_.wholeImg = true;
        for (const std::wstring& name : ops_.Children(img)) {
            if (name == L"info") continue;
            if (ops_.Get(img, name, &sub, &cv) == Kind::Property) SwapLayerOf(sub, layer);
        }
    }

    void SwapLayerOf(const Node& p, Layer layer) {
        if (layer == Layer::Effects) SwapEffectSubtrees(p, 0);
        else                         SwapSubtree(p, 0, Layer::Body);
    }

    // Every canvas under `node` to any depth (the categories disagree on depth), skipping
    // effect nodes on a Body walk.
    void SwapSubtree(const Node& node, int depth, Layer layer) {
        if (depth > 4) return;
        for (const std::wstring& name : ops_.Children(node)) {
            if (layer == Layer::Body && IsEffectPart(name.c_str())) continue;
            Node sub;
            Canvas cv;
            const Kind k = ops_.Get(node, name, &sub, &cv);
            if (k == Kind::Canvas) TintCanvas(node, name, cv);
            else if (k == Kind::Property) SwapSubtree(sub, depth + 1, layer);
        }
    }

    // An effects walk descends only into nodes named effect*. An effect is usually the
    // CANVAS itself (<action>/<frame>/effect beside the weapon), rarely a folder.
    void SwapEffectSubtrees(const Node& node, int depth) {
        if (depth > 4) return;
        for (const std::wstring& name : ops_.Children(node)) {
            const bool isEffect = IsEffectPart(name.c_str());
            Node sub;
            Canvas cv;
            const Kind k = ops_.Get(node, name, &sub, &cv);
            if (k == Kind::Canvas) {
                if (isEffect) TintCanvas(node, name, cv);
                continue;
            }
            if (k != Kind::Property) continue;
            if (isEffect) SwapSubtree(sub, depth + 1, Layer::Effects);
            else          SwapEffectSubtrees(sub, depth + 1);
        }
    }

private:
    void TintCanvas(const Node& parent, const std::wstring& name, const Canvas& cv) {
        ++st_.canvases;
        // Already one of ours: this node was reached a second time in the same pass,
        // through a UOL (a weapon type pointing at another, or a frame at its sibling).
        // Tinting it again would clone the clone and compound the tint.
        if (ops_.IsOurs(cv)) { ++st_.skippedOurs; return; }
        if (ops_.Swap(parent, name, cv)) ++st_.swapped;
    }

    Ops& ops_;
    Stats st_;
};

// ============================================================
// The clone cache: (original canvas, tint) -> tinted clone.
//
// IT HOLDS THE ORIGINAL. The key is the original canvas's ADDRESS, and the client's
// ResMan releases an img nobody has touched for 60 s (MapleHook.cpp sets that retain
// time) and re-parses it on the next access. Without a reference, the original could be
// freed and a DIFFERENT canvas -- another frame or action of the same weapon, another
// weapon type, the weapon just swapped in -- allocated at the same address, and the
// cache would hand back the stale clone with ITS pixels, origin, hand point and z. That
// is the cash weapon drawn detached in front of the character after a weapon swap in
// 1.5.0-rc4. Holding the original pins the address for as long as the entry exists.
//
// It also knows its own clones (IsOurs), which is how a walk recognises a node it has
// already swapped in the same pass. Bounded: Trim() empties it past a cap, and is only
// called between layer builds, never in the middle of one (clearing mid-pass would
// forget which canvases are clones and the walk would clone them again).
// ============================================================
template <class Raw, class Ptr>
class CloneCache {
public:
    // True if (raw, tint) is cached; *clone may be empty (a cached failure).
    bool Find(const Raw* raw, unsigned int tintKey, Ptr* clone) const {
        auto it = map_.find(Key(raw, tintKey));
        if (it == map_.end()) return false;
        *clone = it->second.clone;
        return true;
    }
    void Put(const Raw* rawOriginal, const Ptr& original, unsigned int tintKey,
             const Raw* rawClone, const Ptr& clone) {
        map_[Key(rawOriginal, tintKey)] = Entry{ original, clone };
        if (rawClone) ours_.insert(rawClone);
    }
    bool IsOurs(const Raw* raw) const { return raw && ours_.count(raw) != 0; }
    void Clear() { map_.clear(); ours_.clear(); }
    std::size_t Size() const { return map_.size(); }
    bool Trim(std::size_t cap) {
        if (map_.size() <= cap) return false;
        Clear();
        return true;
    }

private:
    using Key = std::pair<const Raw*, unsigned int>;
    struct Entry {
        Ptr original;   // the reference that keeps the key's address from being reused
        Ptr clone;
    };
    std::map<Key, Entry> map_;
    std::unordered_set<const Raw*> ours_;
};

}  // namespace tintwalk
