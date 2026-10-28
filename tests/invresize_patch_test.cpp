// Resizable inventory (invresize_patch.h): grid maths, the layout operands and the stock bytes.
// At 4x6 every rewritten operand must equal the stock one, so the default size is a byte-for-byte no-op.
// With a MapleStory.exe argument it also checks every guard against that client and the caller counts the
// design depends on (nothing else calls the replaced functions or reads the forced-narrow config).
#include "../src/invresize_patch.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace InvResize;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::uint32_t Le32(const unsigned char* b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

const Guard& GuardAt(std::uint32_t addr) {
    for (const Guard& g : kGuards) {
        if (g.uAddr == addr) return g;
    }
    throw std::runtime_error("no guard at " + std::to_string(addr));
}

// The operand an Imm rewrites, as the stock bytes in its guard hold it.
int StockOperand(const Imm& imm) {
    for (const Guard& g : kGuards) {
        const std::uint32_t at = imm.uInstr + imm.uOperandOffset;
        if (at < g.uAddr || at + imm.nBytes > g.uAddr + g.uSize) continue;
        const unsigned char* p = g.pBytes + (at - g.uAddr);
        return imm.nBytes == 4 ? static_cast<int>(Le32(p)) : static_cast<int>(static_cast<std::int8_t>(p[0]));
    }
    throw std::runtime_error(std::string("operand not covered by a guard: ") + imm.sName);
}

void CheckGeometry() {
    Check(WndW(4) == 175 && WndH(6) == 289, "stock window 175x289");
    for (int c = kMinCols; c <= kMaxCols; ++c) {
        Check(SnapCols(WndW(c)) == c, "a window's own width snaps to its column count");
        Check(SnapCols(WndW(c) + 17) == c && SnapCols(WndW(c) + 18) == c + 1, "columns snap at half the 36px pitch");
    }
    for (int r = kMinRows; r <= kMaxRows; ++r) {
        Check(SnapRows(WndH(r)) == r, "a window's own height snaps to its row count");
        Check(SnapRows(WndH(r) + 16) == r && SnapRows(WndH(r) + 17) == r + 1, "rows snap at half the 34px pitch");
    }

    SlotRect r{};
    Check(SlotRectFor(1, 1, 25, 4, r) && r.left == 8 && r.top == 50 && r.right == 40 && r.bottom == 82, "slot 1 at (8,50)");
    Check(SlotRectFor(24, 1, 25, 4, r) && r.left == 8 + 36 * 3 && r.top == 50 + 34 * 5, "slot 24 bottom-right at 4x6");
    Check(SlotRectFor(9, 1, 97, 8, r) && r.left == 8 && r.top == 50 + 34, "slot 9 starts row 2 at 8 columns");
    Check(!SlotRectFor(25, 1, 25, 4, r) && r.right == 0, "past the visible range is empty");
    Check(!SlotRectFor(0, 1, 25, 4, r), "before the first visible slot is empty");
    Check(SlotRectFor(5, 5, 29, 4, r) && r.left == 8 && r.top == 50, "scrolled: the first visible slot is top-left");

    const int W = WndW(4), H = WndH(6);
    Check(GripAt(W, H, W - 1, 100) == 1, "right edge");
    Check(GripAt(W, H, 50, H - 1) == 2, "bottom edge");
    Check(GripAt(W, H, W - 1, H - 1) == 3, "corner");
    Check(GripAt(W, H, W - 1, 10) == 0, "title bar is never a grip (window move stays native)");
    Check(GripAt(W, H, W - 1, kGridTop - 1) == 0, "grips start at the grid");
    Check(GripAt(W, H, 50, 100) == 0, "inside the grid");
    Check(GripAt(W, H, W, 100) == 0, "outside the window");

    // Locked cells: count includes the unused slot 0, so 24 slots = count 25 = cells 0..23 usable.
    Check(FirstLockedCell(1, 25, 24) == 24, "stock 4x6 with 24 slots locks nothing");
    Check(FirstLockedCell(1, 25, 96) == 24, "8x12 with 24 slots locks from the 25th cell");
    Check(FirstLockedCell(1, 97, 96) == 96, "a full 96-slot tab locks nothing");
    Check(FirstLockedCell(1, 0, 96) == 96, "an unreadable tab locks nothing");
    Check(FirstLockedCell(5, 25, 24) == 20, "scrolled by a row: the last 4 visible cells are past 24 slots");
    Check(FirstLockedCell(30, 25, 24) == 0, "scrolled past the end: everything locked");

    // The tab index is the inventory type: Equip 1 .. Cash 5. Cash must count (1.6.0 drew it empty).
    Check(ValidTab(1) && ValidTab(5), "Equip and Cash are tabs");
    Check(!ValidTab(0) && !ValidTab(6), "nothing outside 1..5");

    // Scrolling reaches every slot of every tab at every size, and no further than the row holding the last slot.
    // 1.6.0's bias was right only at 4 columns: 8x3 with a 28-slot tab had no scrollbar, hiding slots 25-28.
    Check(ImmValue(RangeBias, kStockCols, kStockRows) == -0x16, "stock bias at 4x6");
    Check(ScrollPositions(28, 8, 3) == 2, "8x3, 28 slots: one scroll step to the last row");
    Check(ScrollPositions(96, 5, 3) == 18, "5x3, 96 slots: slot 96 is reachable");
    for (int cols = kMinCols; cols <= kMaxCols; ++cols) {
        for (int rows = kMinRows; rows <= kMaxRows; ++rows) {
            if (!ValidSize(cols, rows)) continue;
            const int cells = cols * rows;
            for (int slots = cells; slots <= 252; ++slots) {
                const int last = ScrollPositions(slots, cols, rows) - 1;   // the furthest scroll position
                const std::string at = std::to_string(cols) + "x" + std::to_string(rows) + " " + std::to_string(slots);
                Check(last * cols + cells >= slots, "last slot reachable at " + at);
                Check(last * cols < slots, "no scrolling past the last row at " + at);
            }
        }
    }

    // New-item flash: centred on the tab. A stock 34px tab at [0,34) gives the stock base 34 (plate x 3).
    Check(FlashBase(0, 34) == 1 * kStockTabPitch, "stock tab 1 keeps the stock flash position");
    Check(FlashBase(34, 68) == 2 * kStockTabPitch, "stock tab 2 keeps the stock flash position");
    Check(FlashBase(0, 88) - kFlashPlateOff + kFlashW / 2 == kTabCtrlLeft + 44, "stretched tab: plate centred");

    // Saved size round-trips; junk reads as stock.
    int sc = 0, sr = 0;
    Check(UnpackSize(PackSize(10, 6), sc, sr) && sc == 10 && sr == 6, "10x6 round-trips");
    Check(UnpackSize(PackSize(16, 7), sc, sr) && sc == 16 && sr == 7, "16x7 round-trips");
    Check(!UnpackSize(PackSize(16, 8), sc, sr) && sc == 4 && sr == 6, "over 120 cells reads as 4x6");
    Check(!UnpackSize(0, sc, sr) && sc == 4 && sr == 6, "zero reads as 4x6");
    Check(!UnpackSize(0x01000A06, sc, sr) && sc == 4 && sr == 6, "high bits set reads as 4x6");

    Check(ValidSize(4, 6) && ValidSize(16, 7) && ValidSize(8, 14) && ValidSize(10, 12), "sizes inside the ceiling");
    Check(!ValidSize(16, 8) && !ValidSize(9, 14) && !ValidSize(3, 6) && !ValidSize(4, 15), "sizes outside");
}

void CheckOperands() {
    // Every Imm is covered by a guard and its stock operand equals the 4x6 value: patching at 4x6 changes nothing.
    for (int id = 0; id < kImmCount; ++id) {
        const Imm& imm = kImms[id];
        Check(StockOperand(imm) == ImmValue(id, kStockCols, kStockRows), std::string("4x6 equals stock: ") + imm.sName);
    }
    // imm8 operands stay encodable at every allowed size.
    for (int c = kMinCols; c <= kMaxCols; ++c) {
        for (int r = kMinRows; r <= kMaxRows; ++r) {
            if (!ValidSize(c, r)) continue;
            for (int id = 0; id < kImmCount; ++id) {
                if (kImms[id].nBytes != 1) continue;
                const int v = ImmValue(id, c, r);
                Check(v >= -128 && v <= 127, std::string("imm8 range: ") + kImms[id].sName);
            }
        }
    }
    // Stride: imul eax, src, cols / inc eax over the 7-byte lea src*4+1; at 4 columns it computes the same.
    for (const Stride& s : kStrides) {
        const Guard& g = GuardAt(s.uAddr);
        Check(g.uSize == kStrideLength && g.pBytes[0] == 0x8D && g.pBytes[1] == 0x04, "stride guard is lea eax,[r*4+1]");
        Check(Le32(g.pBytes + 3) == 1, "lea displacement +1");
        const unsigned char index = (g.pBytes[2] >> 3) & 7, scale = g.pBytes[2] >> 6;
        Check(scale == 2 && (s.modrm & 7) == index && ((s.modrm >> 3) & 7) == 0 && (s.modrm >> 6) == 3,
            "imul reads the lea's index register into eax");
        const auto b = StrideBytes(s.modrm, kStockCols);
        Check(b[0] == 0x6B && b[1] == s.modrm && b[2] == 4 && b[3] == 0x40 && b[4] == 0x90 && b[6] == 0x90,
            "imul r32,r/m32,4 / inc eax / nops");
    }
    Check(kXorEaxNops[0] == 0x33 && kXorEaxNops[1] == 0xC0 && GuardAt(kAddr_NarrowGetterCall).uSize == kXorEaxNops.size(),
        "xor eax,eax replaces the whole 5-byte call");
}

// ---- optional: the real client ------------------------------------------------------------------
std::uint32_t Read32(const std::vector<unsigned char>& d, std::size_t at) {
    Check(at + 4 <= d.size(), "pe read out of range");
    return Le32(d.data() + at);
}

struct Pe {
    std::vector<unsigned char> d;
    std::uint32_t imageBase = 0, textVa = 0, textSize = 0;
    std::size_t textRaw = 0;

    explicit Pe(const char* path) {
        std::ifstream f(path, std::ios::binary);
        Check(f.good(), "cannot open the exe");
        d.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        const std::uint32_t pe = Read32(d, 0x3C);
        Check(Read32(d, pe) == 0x00004550, "not a PE file");
        const unsigned sections = d[pe + 6] | (d[pe + 7] << 8);
        const unsigned optSize = d[pe + 20] | (d[pe + 21] << 8);
        imageBase = Read32(d, pe + 24 + 28);
        for (unsigned i = 0; i < sections; ++i) {
            const std::size_t sh = pe + 24 + optSize + i * 40;
            if (std::memcmp(&d[sh], ".text", 6) != 0) continue;
            textSize = Read32(d, sh + 16);
            textVa = imageBase + Read32(d, sh + 12);
            textRaw = Read32(d, sh + 20);
        }
        Check(textSize != 0, "no .text section");
    }
    const unsigned char* At(std::uint32_t va, std::size_t n) const {
        Check(va >= textVa && va + n <= textVa + textSize, "address outside .text");
        return d.data() + textRaw + (va - textVa);
    }
    int CallersOf(std::uint32_t target) const {
        int n = 0;
        for (std::uint32_t off = 0; off + 5 <= textSize; ++off) {
            const unsigned char op = d[textRaw + off];
            if (op != 0xE8 && op != 0xE9) continue;
            const auto rel = static_cast<std::int32_t>(Le32(&d[textRaw + off + 1]));
            if (textVa + off + 5 + static_cast<std::uint32_t>(rel) == target) ++n;
        }
        return n;
    }
};

void CheckClient(const char* path) {
    const Pe exe(path);
    Check(exe.imageBase == 0x400000, "image base 0x400000");
    for (const Guard& g : kGuards) {
        Check(std::memcmp(exe.At(g.uAddr, g.uSize), g.pBytes, g.uSize) == 0, std::string("stock bytes: ") + g.sName);
    }
    Check(exe.CallersOf(kAddr_GetSlotRect) == 5, "GetSlotRect has 5 callers");
    Check(exe.CallersOf(kAddr_GetSlotPosPoint) == 4, "GetSlotPositionFromPoint has 4 callers");
    Check(exe.CallersOf(0x0049F5DA) == 1, "the m_bFull config getter has one caller (the forced-narrow site)");
    Check(exe.CallersOf(0x0049F62C) == 1, "the m_bFull config setter has one caller (inside ToggleFull)");
    Check(exe.CallersOf(kAddr_ToggleFull) == 1, "ToggleFull has one caller (the expand button)");
    // The replaced functions end the way the hooks return.
    Check(std::memcmp(exe.At(0x0081E3A3, 3), "\xC2\x08\x00", 3) == 0, "GetSlotRect ret 8");
    Check(std::memcmp(exe.At(0x0081DC1D, 3), "\xC2\x08\x00", 3) == 0, "GetSlotPositionFromPoint ret 8");
    Check(*exe.At(0x0081E5D9, 1) == 0xC3, "ToggleFull plain ret");
    Check(std::memcmp(exe.At(0x0081E252, 3), "\xC2\x04\x00", 3) == 0, "Draw ret 4");
    Check(*exe.At(0x004DE470, 1) == 0xC3, "tab layout plain ret");
    Check(std::memcmp(exe.At(0x004DE91B, 3), "\xC2\x04\x00", 3) == 0, "tab lookup ret 4");
    Check(exe.CallersOf(0x0081E5DA) == 4, "the new-item effect has 4 callers, all in CUIItem");
    // The call written at kAddr_NewTabX covers exactly mov ecx,eax / imul ecx,ecx,0x22 and leaves the store.
    Check(std::memcmp(exe.At(kAddr_NewTabX + 5, 3), "\x89\x4D\x90", 3) == 0, "mov [ebp-0x70],ecx follows the call");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        CheckGeometry();
        CheckOperands();
        if (argc > 1) {
            CheckClient(argv[1]);
            std::cout << "invresize: geometry, operands and " << kGuards.size() << " client sites OK\n";
        } else {
            std::cout << "invresize: geometry and operands OK (no exe given)\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "invresize_patch_test FAILED: " << e.what() << "\n";
        return 1;
    }
}
