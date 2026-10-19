#pragma once
#ifdef MAPLEROOT_INTEGRATION
void InstallCrashDiagnostics();
void InitializeMapleRoot();
void VerifyKaentakeHooks(const char* group);
void InitializeMapleRootResources(void* app);
void UpdateMapleRootResolution(int width, int height);
void ShutdownMapleRoot();
#endif
