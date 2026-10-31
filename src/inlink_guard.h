#pragma once

#include "ztl/ztl.h"

// Fills pCanvas with the pixels of the canvas its first link property (_inlink, _outlink or
// source) names, the way the game expects a linked canvas to arrive.
//
// Never throws. This runs inside a map load, and an exception escaping the hook closes the
// client (b103 in rc12: a linked minimap canvas with no origin threw E_POINTER and the client
// died on entry). A link that cannot be resolved, snapshotted, created or filled leaves the
// canvas exactly as it was, and the path is logged, so a broken or unusual link costs a tile,
// not the client.
//
// Returns true when pCanvas now holds the linked canvas.
bool FillLinkedCanvas(IWzCanvasPtr pCanvas);

// The guard resolves links through the resource manager. The stock accessor (wvs/util.h) reads
// a fixed address inside the game exe, which a test binary does not have, so a test can install
// one of its own; passing nullptr restores the stock accessor.
typedef IWzResManPtr& (*InlinkResManAccessor)();
void SetLinkedCanvasResMan(InlinkResManAccessor pfn);
