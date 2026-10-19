#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "wvs/wvsapp.h"
#include "wvs/util.h"
#include "ztl/ztl.h"
#ifdef MAPLEROOT_CASHSHOP_WINDOW
#include "cashshopwnd.h"
#endif
#ifdef MAPLEROOT_MOVEMENT_KEYS
#include "movementkeys.h"
#endif
#ifdef MAPLEROOT_MOUNT_INFO
#include "mountinfo.h"
#endif
#include <algorithm>
#include <vector>
#include <tuple>


static IWzNameSpacePtr g_pCustomNameSpace;
static std::vector<Ztl_bstr_t> g_vecOverrides;

class IWzNameSpaceImpl {
public:
    typedef HRESULT(__stdcall* raw__OnGetLocalObject_t)(IWzNameSpaceImpl*, int, BSTR, int*, VARIANT*);
    inline static raw__OnGetLocalObject_t raw__OnGetLocalObject_orig;

    HRESULT __stdcall raw__OnGetLocalObject_hook(int nIndex, BSTR sPath, int* pnPathUsed, VARIANT* pvRet) {
        HRESULT hr = raw__OnGetLocalObject_orig(this, nIndex, sPath, pnPathUsed, pvRet);
        if (SUCCEEDED(hr) || !g_pCustomNameSpace) {
            return hr;
        }
        if (!std::binary_search(g_vecOverrides.begin(), g_vecOverrides.end(), Ztl_bstr_t(sPath))) {
            return hr;
        }
        return g_pCustomNameSpace->raw__OnGetLocalObject(nIndex, sPath, pnPathUsed, pvRet);
    }
};

class CWzProperty : public IWzProperty {
public:
    typedef HRESULT(__stdcall* raw_Serialize_t)(CWzProperty*, IWzArchive*);
    inline static raw_Serialize_t raw_Serialize_orig;

    HRESULT __stdcall raw_Serialize_hook(IWzArchive* pArchive) {
        HRESULT hr = raw_Serialize_orig(this, pArchive);
        if (FAILED(hr)) return hr;
#ifdef MAPLEROOT_MOVEMENT_KEYS
        InjectMovementKeyIcons(this, pArchive->absoluteUOL);
#endif
        if (!g_pCustomNameSpace) {
            return hr;
        }
        if (!std::binary_search(g_vecOverrides.begin(), g_vecOverrides.end(), pArchive->absoluteUOL)) {
            return hr;
        }
        IWzPropertyPtr pProperty = get_rm()->GetObjectA(L"Custom/" + pArchive->absoluteUOL).GetUnknown();
        IEnumVARIANTPtr pEnum = pProperty->_NewEnum;
        while (true) {
            Ztl_variant_t vNext;
            ULONG uCeltFetched;
            if (FAILED(pEnum->Next(1, &vNext, &uCeltFetched)) || uCeltFetched == 0) {
                break;
            }
            Ztl_bstr_t sNext = V_BSTR(&vNext);
            IUnknownPtr pUnk = pProperty->item[sNext].GetUnknown();
            IWzPropertyPtr pSub;
            if (!pUnk || FAILED(pUnk->QueryInterface(&pSub))) {
                this->Add(sNext, pProperty->item[sNext], false);
            }
        }
        return S_OK;
    }
};


void CWvsApp::InitializeResMan_hook() {
    DEBUG_MESSAGE("CWvsApp::InitializeResMan");
#ifdef MAPLEROOT_INTEGRATION
    InitializeMapleRootResources(this);
#else
    CWvsApp::InitializeResMan(this);
#endif

    // add custom namespace to root
    IWzWritableNameSpacePtr pWritableRoot;
    if (FAILED(get_root()->QueryInterface(&pWritableRoot))) {
        ErrorMessage("Failed to cast root namespace");
        return;
    }
    IWzNameSpacePtr pNameSpace;
    PcCreateObject<IWzNameSpacePtr>(L"NameSpace", pNameSpace, nullptr);
    Ztl_variant_t vResult;
    pWritableRoot->AddObject(L"Custom", static_cast<IUnknown*>(pNameSpace), &vResult);
    g_pCustomNameSpace = vResult.GetUnknown();

    // load Custom.wz from file system
    IWzFileSystemPtr fs;
    PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fs, nullptr);
    char sStartPath[MAX_PATH];
    GetModuleFileNameA(nullptr, sStartPath, MAX_PATH);
    Dir_BackSlashToSlash(sStartPath);
    Dir_upDir(sStartPath);
    fs->Init(sStartPath);


    IWzPackagePtr pPackage;
    PcCreateObject<IWzPackagePtr>(L"NameSpace#Package", pPackage, nullptr);
    IWzSeekableArchivePtr pArchive = fs->item[L"Custom.wz"].GetUnknown();
    pPackage->Init(L"83", L"Custom", pArchive);
    g_pCustomNameSpace->Mount(L"/", pPackage, 1);

    // iterate custom namespace
    std::vector<std::tuple<Ztl_bstr_t, IEnumVARIANTPtr>> stack;
    stack.emplace_back(L"", g_pCustomNameSpace->_NewEnum);
    while (!stack.empty()) {
        auto [sPath, pEnum] = stack.back();
        stack.pop_back();

        while (true) {
            Ztl_variant_t vNext;
            ULONG uCeltFetched;
            if (FAILED(pEnum->Next(1, &vNext, &uCeltFetched)) || uCeltFetched == 0) {
                break;
            }
            Ztl_bstr_t sUOL = (sPath.length() > 0 ? sPath + L"/" : L"") + V_BSTR(&vNext);
            Ztl_variant_t vObj = get_rm()->GetObjectA(L"Custom/" + sUOL);
            IUnknownPtr pUnk = vObj.GetUnknown();
            if (pUnk) {
                IWzNameSpacePtr pSub;
                if (SUCCEEDED(pUnk->QueryInterface(&pSub))) {
                    stack.emplace_back(sUOL, pSub->_NewEnum);
                    continue;
                }
                IWzPropertyPtr pProp;
                if (SUCCEEDED(pUnk->QueryInterface(&pProp))) {
                    stack.emplace_back(sUOL, pProp->_NewEnum);
                }
            }
            g_vecOverrides.push_back(sUOL);
        }
    }
    std::sort(g_vecOverrides.begin(), g_vecOverrides.end()); // uses operator<

    // NameSpace.dll - try resolving from g_pCustomNameSpace
    IWzNameSpaceImpl::raw__OnGetLocalObject_orig = static_cast<IWzNameSpaceImpl::raw__OnGetLocalObject_t>(GetAddressByPattern("NAMESPACE.DLL", "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 81 EC 80"));
    ATTACH_HOOK(IWzNameSpaceImpl::raw__OnGetLocalObject_orig, IWzNameSpaceImpl::raw__OnGetLocalObject_hook);

    // PCOM.dll - patch CWzProperty objects during serialization
    CWzProperty::raw_Serialize_orig = static_cast<CWzProperty::raw_Serialize_t>(GetAddressByPattern("PCOM.DLL", "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 EC 68"));
    ATTACH_HOOK(CWzProperty::raw_Serialize_orig, CWzProperty::raw_Serialize_hook);
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("Custom.wz serialization and namespace");
    DebugMessage("SESSION_OVERLAY_READY entries=%u", static_cast<unsigned>(g_vecOverrides.size()));
#endif
}

#ifdef MAPLEROOT_WEATHER
extern void Weather_Shutdown();
#endif
void CWvsApp::CleanUp_hook() {
    // Release the extension while the game's resource manager and PCOM objects
    // are still alive. Native cleanup destroys both before it returns.
    DebugMessage("SESSION_CLEANUP_BEGIN");
#ifdef MAPLEROOT_MOUNT_INFO
    ShutdownMountInfo();
#endif
#if defined(MAPLEROOT_CASHSHOP_WINDOW) || defined(MAPLEROOT_WEATHER) || defined(MAPLEROOT_DAILY_CHECKIN)
    ShutdownCustomPackets();
#endif
#ifdef MAPLEROOT_WEATHER
    Weather_Shutdown();
#endif
#ifdef MAPLEROOT_CASHSHOP_WINDOW
    CashShopWnd_Shutdown();
#endif
#ifdef MAPLEROOT_INTEGRATION
    ShutdownMapleRoot();
#endif
    ReleaseItemEffectResources();
    std::vector<Ztl_bstr_t>().swap(g_vecOverrides);
    if (g_pCustomNameSpace && get_root()) {
        IWzWritableNameSpacePtr writable;
        if (SUCCEEDED(get_root()->QueryInterface(&writable))) {
            writable->Unlink(L"Custom");
        }
    }
    g_pCustomNameSpace = nullptr;
    DebugMessage("SESSION_CUSTOM_RESOURCES_RELEASED");
    CWvsApp::CleanUp(this);
    ReleaseClientInlinkResources();
    DebugMessage("SESSION_CLEANUP_COMPLETE");
}


void AttachResManMod() {
    ATTACH_HOOK(CWvsApp::InitializeResMan, CWvsApp::InitializeResMan_hook);
    ATTACH_HOOK(CWvsApp::CleanUp, CWvsApp::CleanUp_hook);
}
