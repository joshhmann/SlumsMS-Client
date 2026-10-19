#include "Gfx.h"
#include <windows.h>
#include <psapi.h>

typedef void(__fastcall* CStage_OnSetField_t)(void* pThis, void* pEdx, void* pInPacket);
CStage_OnSetField_t CStage_OnSetField = (CStage_OnSetField_t)0x0776020;

struct CONFIG_SYSOPT
{
	int nSysOpt_Video;
	int nSysOpt_BGMVol;
	int bSysOpt_BGMMute;
	int nSysOpt_SEVol;
	int bSysOpt_SEMute;
	int nSysOpt_ScreenShot;
	int nSysOpt_MouseSpeed;
	int nSysOpt_HPFlash;
	int nSysOpt_MPFlash;
	int bSysOpt_Tremble;
	int nSysOpt_MobInfo;
	//int bSysOpt_LargeScreen;
	int bSysOpt_WindowedMode;
	//int bSysOpt_Minimap_Normal;
};

CONFIG_SYSOPT* GetSysOpt(void* pCfg) {
	uint8_t* p = (uint8_t*)pCfg;
	return (CONFIG_SYSOPT*)&p[100];
}

typedef void(__thiscall* ApplySysOpt_t)(void* pThis, void* pSysOpt, int nOpt);
void** CConfig_Instance = (void**)0x00BEBF9C;
ApplySysOpt_t ApplySysOpt = (ApplySysOpt_t)0x049EA33;


bool IsMemoryUsageAbove(size_t megabytes)
{
	// Get a handle to the current process
	HANDLE hProcess = GetCurrentProcess();
	if (hProcess == NULL)
	{
		return false;
	}

	// Create a structure to hold memory info
	PROCESS_MEMORY_COUNTERS pmc;

	// Get memory info for the current process
	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
	{
		// Close the process handle
		CloseHandle(hProcess);

		SIZE_T memoryUsage = pmc.WorkingSetSize;  // WorkingSetSize in bytes

		// Convert megabytes to bytes and compare
		return memoryUsage > (megabytes * 1024 * 1024);
	}

	// Close the process handle and return false in case of failure
	CloseHandle(hProcess);
	return false;
}

void ResetGfx()
{
	auto pCfg = *CConfig_Instance;
	auto pSysOpt = GetSysOpt(pCfg);
	auto oldVideo = pSysOpt->nSysOpt_Video;

	pSysOpt->nSysOpt_Video = oldVideo?0:1;

	ApplySysOpt(*CConfig_Instance, pSysOpt, 1);

	pSysOpt->nSysOpt_Video = oldVideo;
	ApplySysOpt(*CConfig_Instance, pSysOpt, 1);
}

void Hook_CStage_OnSetField(bool enable)
{
	static int counter = 0;
	static CStage_OnSetField_t CStage_OnSetField_Hook = [](void* pThis, void* pEdx, void* pInPacket) -> void
		{
			CStage_OnSetField(pThis, pEdx, pInPacket);
			counter++;
			if (counter >= GFX_RESET_COUNTER) {
				if (IsMemoryUsageAbove(1024)) {
					ResetGfx();
				}
				counter = 0;
			}
		};
	SetHook(enable, reinterpret_cast<void**>(&CStage_OnSetField), CStage_OnSetField_Hook);
}

void EnableLFH()
{
	DWORD HeapInformation = 2;
	HANDLE ProcessHeap = GetProcessHeap();
	if (!HeapSetInformation(ProcessHeap, HeapCompatibilityInformation, &HeapInformation, 4u))
	{
		//TODO log if failed
	}
}
