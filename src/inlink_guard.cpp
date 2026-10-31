#include "pch.h"
#include "inlink_guard.h"
#include "debug.h"
#include "wvs/util.h"
#include "ztl/ztl.h"

static IWzResManPtr& StockResMan() {
    return get_rm();
}

static InlinkResManAccessor g_pfnResMan = &StockResMan;

void SetLinkedCanvasResMan(InlinkResManAccessor pfn) {
    g_pfnResMan = pfn ? pfn : &StockResMan;
}

// Every WzLib call below throws _com_error when it fails: GetObjectA on a path that is not
// there, GetSnapshot on a canvas with no image, Create on a size the allocator refuses. The
// original code called them unguarded from inside a hook that runs during the map load, so any
// failure left the process instead of the canvas. Logging and returning is enough: the game
// draws an empty canvas and carries on.
bool FillLinkedCanvas(IWzCanvasPtr pCanvas) {
    if (!pCanvas) {
        return false;
    }
    const wchar_t* asLinkProperty[] = {
        L"_inlink",
        L"_outlink",
        L"source",
    };
    size_t nLinkProperty = sizeof(asLinkProperty) / sizeof(asLinkProperty[0]);
    try {
        IWzPropertyPtr pProperty = pCanvas->property;
        if (!pProperty) {
            return false;
        }
        for (size_t i = 0; i < nLinkProperty; ++i) {
            Ztl_variant_t vLink = pProperty->item[asLinkProperty[i]];
            if (V_VT(&vLink) != VT_BSTR) {
                continue;
            }

            // Try resolving source canvas
            IWzCanvasPtr pSource;
            IUnknownPtr pUnknown = g_pfnResMan()->GetObjectA(V_BSTR(&vLink)).GetUnknown();
            if (!pUnknown || FAILED(pUnknown->QueryInterface(&pSource)) || !pSource) {
                DEBUG_MESSAGE("inlink: unresolved %ls=\"%ls\"", asLinkProperty[i], V_BSTR(&vLink));
                continue;
            }

            int nWidth = 0;
            int nHeight = 0;
            int nFormat = CP_UNKNOWN;
            int nMagLevel = 0;
            if (FAILED(pSource->GetSnapshot(&nWidth, &nHeight, nullptr, nullptr, (CANVAS_PIXFORMAT*)&nFormat, &nMagLevel))
                || nWidth <= 0 || nHeight <= 0) {
                DEBUG_MESSAGE("inlink: no snapshot %ls=\"%ls\" size=%dx%d", asLinkProperty[i], V_BSTR(&vLink), nWidth, nHeight);
                continue;
            }
            IWzRawCanvas* pRaw = nullptr;
            if (FAILED(pSource->get_rawCanvas(0, 0, &pRaw)) || !pRaw) {
                DEBUG_MESSAGE("inlink: no raw canvas %ls=\"%ls\" size=%dx%d", asLinkProperty[i], V_BSTR(&vLink), nWidth, nHeight);
                continue;
            }

            // Create target canvas
            if (FAILED(pCanvas->raw_Create((unsigned int)nWidth, (unsigned int)nHeight,
                                           Ztl_variant_t(nMagLevel), Ztl_variant_t(nFormat)))) {
                DEBUG_MESSAGE("inlink: cannot create %dx%d from %ls=\"%ls\"", nWidth, nHeight, asLinkProperty[i], V_BSTR(&vLink));
                continue;
            }
            if (FAILED(pCanvas->raw_AddRawCanvas(0, 0, pRaw))) {
                DEBUG_MESSAGE("inlink: cannot fill %dx%d from %ls=\"%ls\"", nWidth, nHeight, asLinkProperty[i], V_BSTR(&vLink));
                continue;
            }

            // Set target origin. Some linked canvases have none (minimaps: Map2/211060100.img/miniMap/canvas links
            // to Map3/306010100.img), and a null origin here throws E_POINTER and closes the client on map load.
            IWzVector2DPtr pOrigin = pProperty->item[L"origin"].GetUnknown();
            if (pOrigin) {
                pCanvas->cx = pOrigin->x;
                pCanvas->cy = pOrigin->y;
            }
            return true;
        }
    } catch (const _com_error& e) {
        DEBUG_MESSAGE("inlink: gave up (0x%08lX)", (unsigned long)e.Error());
    }
    return false;
}
