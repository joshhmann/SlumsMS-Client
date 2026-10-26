#pragma once
#ifdef MAPLEROOT_INTEGRATION
void InstallCrashDiagnostics();
void InitializeMapleRoot();
void VerifyKaentakeHooks(const char* group);
void InitializeMapleRootResources(void* app);
void UpdateMapleRootResolution(int width, int height);
void ShutdownMapleRoot();
long long MapleRoot_CurrentExp();     // MapleRoot 64-bit EXP (mapleroot/ijl15/MapleHook.cpp)
long long MapleRoot_NextLevelExp();   // its EXP table for the current level; 0 outside the field
#endif
