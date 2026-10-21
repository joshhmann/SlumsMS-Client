#pragma once
// coloringprism_text.h: the Coloring Prism's item name and description (MAPLEROOT_COLOR_PRISM).
//
// String/Cash.img is stock and Custom.wz cannot add 5782000 to it, so CItemInfo answers "" for the
// prism's name and description. coloringprism_hooks.cpp fills them in from here, and only when the
// stock lookup came back empty. The text matches the server's tools/colorprism/install_wz.py
// (NAME, DESC): change both together. Pure, so tests/coloringprism_text_test.cpp can check it.
#include <cstring>

constexpr int kColorPrismTextItemId = 5782000;
constexpr const char* kColorPrismName = "Coloring Prism";
constexpr const char* kColorPrismDesc =
    "Double-click to open the Coloring Prism and dye a Cash equip, its glow, your hair or your eyes. "
    "Each confirmed dye or restore uses one prism.";

// The text CItemInfo's string worker should answer for (nItemID, sKey), or nullptr to keep the stock
// answer. sKey is the worker's property name ("name" from GetItemName, "desc" from GetItemDesc).
inline const char* ColorPrism_ItemTextOverride(int nItemID, const char* sKey) {
    if (nItemID != kColorPrismTextItemId || !sKey) return nullptr;
    if (std::strcmp(sKey, "name") == 0) return kColorPrismName;
    if (std::strcmp(sKey, "desc") == 0) return kColorPrismDesc;
    return nullptr;
}
