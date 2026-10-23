// Coloring Prism swap walk (src/weapontint_walk.h) against a fake WZ tree shaped like
// the real cash weapon imgs (01702200.img: weapon type 30 drawn, 24 other types UOL'd
// to it, no rope / ladder / sit frames anywhere).
//
// 1.5.0-rc4 regressions this pins:
//   * grabbing a rope dyed the WHOLE cash weapon img (every type, action and frame),
//     freezing the client -- now nothing is walked for an action the item lacks;
//   * the UOL'd types re-tinted the clones the first type had just swapped in (a chain
//     of clone-of-clone per canvas) -- now each canvas is cloned once per tint;
//   * the clone cache keyed on a canvas ADDRESS without holding the canvas, so a freed
//     and reused address returned another frame's clone (the floating cash weapon) --
//     the cache now keeps every original it is keyed on alive.
#include "../src/weapontint_walk.h"

#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

// ---------------------------------------------------------------- fake tree
struct FakeCanvas {
    std::wstring art;      // what it depicts, e.g. L"30/stand1/0/weapon"
    int tintDepth = 0;     // 0 = original, 1 = tinted once, 2 = a clone of a clone...
};
using CanvasPtr = std::shared_ptr<FakeCanvas>;

struct FakeProp;
using PropPtr = std::shared_ptr<FakeProp>;

struct Child {
    std::wstring name;
    CanvasPtr canvas;      // a canvas leaf
    PropPtr prop;          // a property node
    std::wstring uol;      // a UOL, resolved at access time relative to the parent
};

struct FakeProp {
    std::vector<Child> kids;
    FakeProp* parent = nullptr;
    Child* Find(const std::wstring& n) {
        for (Child& c : kids) if (c.name == n) return &c;
        return nullptr;
    }
    PropPtr Add(const std::wstring& n) {
        auto p = std::make_shared<FakeProp>();
        p->parent = this;
        kids.push_back(Child{ n, nullptr, p, L"" });
        return p;
    }
    CanvasPtr AddCanvas(const std::wstring& n, const std::wstring& art) {
        auto c = std::make_shared<FakeCanvas>();
        c->art = art;
        kids.push_back(Child{ n, c, nullptr, L"" });
        return c;
    }
    void AddUol(const std::wstring& n, const std::wstring& path) {
        kids.push_back(Child{ n, nullptr, nullptr, path });
    }
};

// Resolve a relative UOL path from `from` the way ResMan does: ".." is the parent.
static bool ResolveUol(FakeProp* from, const std::wstring& path, CanvasPtr* cv, PropPtr* sub) {
    FakeProp* cur = from;
    size_t at = 0;
    while (at <= path.size()) {
        const size_t slash = path.find(L'/', at);
        const std::wstring seg = path.substr(at, slash == std::wstring::npos ? std::wstring::npos : slash - at);
        const bool last = slash == std::wstring::npos;
        if (seg == L"..") {
            cur = cur->parent;
            if (!cur) return false;
        } else {
            Child* c = cur->Find(seg);
            if (!c) return false;
            if (!c->uol.empty()) {
                CanvasPtr cv2; PropPtr sub2;
                if (!ResolveUol(cur, c->uol, &cv2, &sub2)) return false;
                if (last) { *cv = cv2; *sub = sub2; return true; }
                if (!sub2) return false;
                cur = sub2.get();
            } else if (last) {
                *cv = c->canvas; *sub = c->prop; return true;
            } else {
                if (!c->prop) return false;
                cur = c->prop.get();
            }
        }
        if (last) break;
        at = slash + 1;
    }
    return false;
}

struct SwapRec { PropPtr parent; std::wstring name; CanvasPtr original; std::wstring originalUol; };

struct FakeOps {
    using Node = PropPtr;
    using Canvas = CanvasPtr;
    tintwalk::CloneCache<FakeCanvas, CanvasPtr>& cache;
    unsigned int tint;
    std::vector<SwapRec> swaps;
    int clonesMade = 0;

    std::vector<std::wstring> Children(const Node& n) {
        std::vector<std::wstring> v;
        for (const Child& c : n->kids) v.push_back(c.name);
        return v;
    }
    tintwalk::Kind Get(const Node& parent, const std::wstring& name, Node* sub, Canvas* cv) {
        Child* c = parent->Find(name);
        if (!c) return tintwalk::Kind::None;
        CanvasPtr rc = c->canvas; PropPtr rp = c->prop;
        if (!c->uol.empty() && !ResolveUol(parent.get(), c->uol, &rc, &rp)) return tintwalk::Kind::None;
        if (rc) { *cv = rc; return tintwalk::Kind::Canvas; }
        if (rp) { *sub = rp; return tintwalk::Kind::Property; }
        return tintwalk::Kind::None;
    }
    bool IsOurs(const Canvas& cv) { return cache.IsOurs(cv.get()); }
    bool Swap(const Node& parent, const std::wstring& name, const Canvas& orig) {
        CanvasPtr clone;
        if (!cache.Find(orig.get(), tint, clone)) {
            clone = std::make_shared<FakeCanvas>(*orig);
            clone->tintDepth = orig->tintDepth + 1;
            ++clonesMade;
            cache.Put(orig.get(), orig, tint, clone.get(), clone);
        }
        if (!clone) return false;
        Child* c = parent->Find(name);
        swaps.push_back(SwapRec{ parent, name, orig, c->uol });
        // Writing a canvas into a UOL node replaces the UOL, exactly as in WZ.
        c->uol.clear();
        c->canvas = clone;
        c->prop = nullptr;
        return true;
    }
    void Restore() {
        for (auto it = swaps.rbegin(); it != swaps.rend(); ++it) {
            Child* c = it->parent->Find(it->name);
            c->canvas = it->original;
        }
        swaps.clear();
    }
};

// 01702200.img in miniature: type 30 is drawn, 31..47 point at it, and nothing has
// rope / ladder frames. stand1/2/weapon is a UOL to its sibling frame, as many are.
struct CashWeapon {
    PropPtr img = std::make_shared<FakeProp>();
    std::vector<CanvasPtr> originals;   // keep handles to check the restore
    int totalCanvases = 0;
    CashWeapon() {
        auto info = img->Add(L"info");
        info->AddCanvas(L"icon", L"info/icon");
        auto t30 = img->Add(L"30");
        for (const wchar_t* act : { L"stand1", L"walk1", L"alert", L"swingO1", L"jump" }) {
            auto a = t30->Add(act);
            for (int f = 0; f < 3; ++f) {
                auto fr = a->Add(std::to_wstring(f));
                const std::wstring base = std::wstring(L"30/") + act + L"/" + std::to_wstring(f);
                if (f == 2) {
                    fr->AddUol(L"weapon", L"../1/weapon");
                } else {
                    originals.push_back(fr->AddCanvas(L"weapon", base + L"/weapon"));
                    ++totalCanvases;
                }
                originals.push_back(fr->AddCanvas(L"effect", base + L"/effect"));
                ++totalCanvases;
            }
        }
        for (int type : { 31, 32, 33, 37, 38, 40, 41, 42, 43, 44, 45, 46, 47 }) {
            auto t = img->Add(std::to_wstring(type));
            for (const wchar_t* act : { L"stand1", L"walk1", L"alert", L"swingO1", L"jump" })
                t->AddUol(act, std::wstring(L"../30/") + act);
        }
    }
    // What the client would read at 30/stand1/<f>/<part>, through any UOL.
    CanvasPtr At(const std::wstring& path) {
        CanvasPtr cv; PropPtr sub;
        ResolveUol(img.get(), path, &cv, &sub);
        return cv;
    }
};

static void TestRopeWalksNothing() {
    CashWeapon w;
    tintwalk::CloneCache<FakeCanvas, CanvasPtr> cache;
    FakeOps ops{ cache, 0xD7091146u };
    tintwalk::Walker<FakeOps> walk(ops);
    for (const wchar_t* act : { L"rope", L"ladder", L"sit", L"swim" }) {
        walk.SwapItem(w.img, act, tintwalk::Layer::Body, tintwalk::WalkWholeImgWhenUnmatched(L"Weapon"));
        walk.SwapItem(w.img, act, tintwalk::Layer::Effects, tintwalk::WalkWholeImgWhenUnmatched(L"Weapon"));
    }
    CHECK(walk.stats().canvases == 0, "rope/ladder walked %d canvases of a cash weapon", walk.stats().canvases);
    CHECK(ops.clonesMade == 0, "rope/ladder made %d clones", ops.clonesMade);
    CHECK(!walk.stats().matched && !walk.stats().wholeImg, "cash weapon has no rope frames");

    // What rc4 did: the whole img, every type, every action -- the freeze.
    tintwalk::CloneCache<FakeCanvas, CanvasPtr> cache2;
    FakeOps old{ cache2, 0xD7091146u };
    tintwalk::Walker<FakeOps> oldWalk(old);
    oldWalk.SwapItem(w.img, L"rope", tintwalk::Layer::Body, /*rc4 behaviour*/ true);
    CHECK(oldWalk.stats().wholeImg && oldWalk.stats().canvases >= 15,
          "the rc4 fallback should have reached every weapon frame (%d)", oldWalk.stats().canvases);
    old.Restore();
    std::printf("rope on a cash weapon: the rc4 whole-img fallback visits %d canvases (%d clones); fixed walk visits %d\n",
                oldWalk.stats().canvases, old.clonesMade, walk.stats().canvases);
}

static void TestUolTypesCloneOnce() {
    CashWeapon w;
    tintwalk::CloneCache<FakeCanvas, CanvasPtr> cache;
    FakeOps ops{ cache, 0xD7091146u };
    {
        tintwalk::Walker<FakeOps> walk(ops);
        walk.SwapItem(w.img, L"stand1", tintwalk::Layer::Body, false);
        // 30/stand1 has weapon canvases in frames 0 and 1 (frame 2 is a UOL to frame 1).
        CHECK(ops.clonesMade == 2, "stand1 body: expected 2 clones (one per weapon canvas), made %d", ops.clonesMade);
        CHECK(walk.stats().skippedOurs > 0, "the 13 UOL'd types should find our clones, not re-tint them");
        // While swapped, what the client builds from is tinted exactly ONCE.
        for (int f = 0; f < 3; ++f) {
            CanvasPtr c = w.At(L"30/stand1/" + std::to_wstring(f) + L"/weapon");
            CHECK(c && c->tintDepth == 1, "30/stand1/%d/weapon tint depth %d (must be 1, not a clone of a clone)",
                  f, c ? c->tintDepth : -1);
            CanvasPtr e = w.At(L"30/stand1/" + std::to_wstring(f) + L"/effect");
            CHECK(e && e->tintDepth == 0, "a body walk must not dye the effect canvas");
        }
        CanvasPtr viaUol = w.At(L"47/stand1/0/weapon");
        CHECK(viaUol && viaUol->tintDepth == 1, "47/stand1 resolves to the once-tinted clone");
    }
    ops.Restore();
    for (const CanvasPtr& o : w.originals) {
        CanvasPtr now = w.At(o->art);
        CHECK(now == o, "restore must put the original back at %ls", o->art.c_str());
    }
    CHECK(w.At(L"30/stand1/2/weapon") == w.At(L"30/stand1/1/weapon"), "frame 2 still shows frame 1's art");

    // Second build of the same action: all cache hits.
    const int before = ops.clonesMade;
    {
        tintwalk::Walker<FakeOps> walk(ops);
        walk.SwapItem(w.img, L"stand1", tintwalk::Layer::Body, false);
    }
    ops.Restore();
    CHECK(ops.clonesMade == before, "a rebuild of stand1 cloned again (%d new)", ops.clonesMade - before);

    // Effects tab: only the effect canvases, once each.
    FakeOps fx{ cache, 0xFFC80000u };
    {
        tintwalk::Walker<FakeOps> walk(fx);
        walk.SwapItem(w.img, L"stand1", tintwalk::Layer::Effects, false);
        CHECK(fx.clonesMade == 3, "stand1 effects: expected 3 clones, made %d", fx.clonesMade);
        CHECK(w.At(L"30/stand1/0/weapon")->tintDepth == 0, "an effects walk must not dye the weapon");
    }
    fx.Restore();
}

static void TestAccessoryFallback() {
    // Emotion-keyed accessory without a node for the action: every emotion, never info.
    auto img = std::make_shared<FakeProp>();
    img->Add(L"info")->AddCanvas(L"icon", L"info/icon");
    for (const wchar_t* emo : { L"blink", L"hit", L"smile" })
        img->Add(emo)->AddCanvas(L"0", std::wstring(emo) + L"/0");
    tintwalk::CloneCache<FakeCanvas, CanvasPtr> cache;
    FakeOps ops{ cache, 1u };
    tintwalk::Walker<FakeOps> walk(ops);
    walk.SwapItem(img, L"rope", tintwalk::Layer::Body, tintwalk::WalkWholeImgWhenUnmatched(L"Accessory"));
    CHECK(walk.stats().wholeImg && ops.clonesMade == 3, "accessory fallback dyed %d of 3 emotions", ops.clonesMade);
    ops.Restore();
}

static void TestRules() {
    CHECK(tintwalk::WalkWholeImgWhenUnmatched(L"Accessory"), "Accessory falls back");
    CHECK(tintwalk::WalkWholeImgWhenUnmatched(L"accessory"), "case-insensitive");
    for (const wchar_t* s : { L"Weapon", L"Coat", L"Longcoat", L"Cap", L"Cape", L"Shield", L"Glove", L"Shoes", L"Pants", L"Ring", L"Acc" })
        CHECK(!tintwalk::WalkWholeImgWhenUnmatched(s), "%ls must not walk its whole img", s);
    CHECK(!tintwalk::WalkWholeImgWhenUnmatched(nullptr), "null subdir");
    CHECK(tintwalk::IsEffectPart(L"effect") && tintwalk::IsEffectPart(L"Effect2") && tintwalk::IsEffectPart(L"EFFECT"), "effect names");
    CHECK(!tintwalk::IsEffectPart(L"weapon") && !tintwalk::IsEffectPart(L"eff") && !tintwalk::IsEffectPart(nullptr), "not effect names");
}

static void TestCacheHoldsOriginals() {
    tintwalk::CloneCache<FakeCanvas, CanvasPtr> cache;
    std::weak_ptr<FakeCanvas> weakOrig;
    const FakeCanvas* rawOrig = nullptr;
    {
        auto orig = std::make_shared<FakeCanvas>();
        orig->art = L"30/shoot1/1/weapon";
        auto clone = std::make_shared<FakeCanvas>(*orig);
        cache.Put(orig.get(), orig, 7u, clone.get(), clone);
        weakOrig = orig;
        rawOrig = orig.get();
    }   // ResMan drops the img: the tree's reference is gone
    CHECK(!weakOrig.expired(), "the cache must keep the original alive, or its address can be reused "
                               "by another canvas and hit this entry (floating weapon)");
    // A canvas parsed afterwards can therefore never land on the cached address.
    std::vector<CanvasPtr> fresh;
    for (int i = 0; i < 256; ++i) {
        fresh.push_back(std::make_shared<FakeCanvas>());
        CHECK(fresh.back().get() != rawOrig, "a new canvas reused a cached original's address");
    }
    CanvasPtr hit;
    CHECK(cache.Find(rawOrig, 7u, hit) && hit && hit->art == L"30/shoot1/1/weapon", "cached clone");
    CHECK(!cache.Find(rawOrig, 8u, hit), "another tint is another entry");
    CHECK(cache.IsOurs(hit.get()) && !cache.IsOurs(rawOrig), "IsOurs knows clones from originals");
    // Failures are cached too, and are not ours.
    auto bad = std::make_shared<FakeCanvas>();
    cache.Put(bad.get(), bad, 7u, nullptr, nullptr);
    CanvasPtr none = std::make_shared<FakeCanvas>();
    CHECK(cache.Find(bad.get(), 7u, none) && !none, "a cached failure reads back as empty");
    CHECK(!cache.Trim(2) && cache.Size() == 2, "under the cap nothing is dropped");
    CHECK(cache.Trim(1) && cache.Size() == 0 && !cache.IsOurs(hit.get()), "over the cap it empties");
    CHECK(weakOrig.expired() || weakOrig.use_count() == 0, "a trimmed entry releases its original");
}

int main() {
    TestRules();
    TestRopeWalksNothing();
    TestUolTypesCloneOnce();
    TestAccessoryFallback();
    TestCacheHoldsOriginals();
    if (g_fail) {
        std::printf("%d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("weapontint walk: all checks passed\n");
    return 0;
}
