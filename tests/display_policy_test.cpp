// Ticket 120: display toggles (display_policy.h).
#include "../src/display_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace Display;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        Prefs d;
        for (Category c : {Category::Back, Category::Tile, Category::Object, Category::SkillEffect, Category::Pet,
                 Category::Summon, Category::CashEffect, Category::Other}) {
            for (Owner o : {Owner::Mine, Owner::Others, Owner::Map}) {
                const LayerState s = Decide(c, o, d);
                Check(s.visible && s.alpha == 255, "defaults are stock");
            }
        }
        Prefs p;
        p.hideBack = true; p.othersEffectPct = 40; p.petPct = 0; p.hideOthersCash = true; p.maxAnimatedOthers = 3;
        Check(!Decide(Category::Back, Owner::Map, p).visible, "back hidden");
        Check(Decide(Category::Tile, Owner::Map, p).visible, "tiles untouched");
        Check(Decide(Category::SkillEffect, Owner::Others, p).alpha == 102, "others' effects at 40%");
        Check(Decide(Category::SkillEffect, Owner::Mine, p).alpha == 255, "mine stay solid");
        Check(!Decide(Category::Pet, Owner::Others, p).visible && Decide(Category::Pet, Owner::Mine, p).visible, "only others' pets hide");
        Check(!Decide(Category::CashEffect, Owner::Others, p).visible && Decide(Category::CashEffect, Owner::Mine, p).visible, "cash effects");
        Check(ApplyAlpha(0x80FF0000u, 128) == 0x40FF0000u, "alpha multiplies");
        Check(MayAnimate(2, p) && !MayAnimate(3, p) && MayAnimate(50, d), "animation cap");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "display: ok\n";
    return 0;
}
