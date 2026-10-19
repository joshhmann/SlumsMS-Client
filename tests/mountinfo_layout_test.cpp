#include "../src/mountinfo_layout.h"
#include <cmath>
int main() {
    MountBounds giant{-174,-200,174,37}; auto a=FitMountPreview(giant);
    if (a.scale <= 0 || a.scale >= 1 || a.originX+giant.left*a.scale < 9-0.001
        || a.originX+giant.right*a.scale > 96+0.001 || a.originY+giant.top*a.scale < 182-0.001
        || std::abs(a.originY+giant.bottom*a.scale-259)>0.001) return 1;
    auto small=FitMountPreview({-35,-50,35,10}); if (small.scale!=1) return 2;
    if (FitMountPreview({0,0,0,1}).scale!=0) return 3;
    MountBounds asymmetric{-15,-100,300,20}; auto b=FitMountPreview(asymmetric);
    if (b.originX+asymmetric.left*b.scale < 9-0.001 || b.originX+asymmetric.right*b.scale > 96+0.001) return 4;
    // Red Draco's front/mid/rear artwork uses distinct origins. Its body extends
    // below the front part, so fit the union rather than the head/arms alone.
    MountBounds draco{-62,-115,62,37}; auto d=FitMountPreview(draco);
    if (std::abs(d.scale-77.0/152.0)>0.0001 || d.originY-115*d.scale < 182-0.001
        || d.originY+37*d.scale > 259+0.001) return 5;
    return 0;
}
