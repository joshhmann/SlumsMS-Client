#include <windows.h>
#include <cstdint>
#include <stdio.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <typeinfo>
#include <sstream>
#include <conio.h>
#include <intrin.h>
#include <WinUser.h>
#include <timeapi.h>
#include <rpcdce.h>
#include <rpc.h>
#include <memoryapi.h>
#include <winapifamily.h>
#include <format>
#include <regex>
#include <locale>
#include <map>
#include <list>
#include <mutex>
#include <memory>
#include <SDKDDKVer.h>
#include <dos.h>
#include <chrono>
#include <cstdio>
#include <stdint.h>
#include <ctime>
#include <thread>
#include <future>
#include <time.h>
#include <comutil.h>
#include "Funcs.h"
#include "Config.h"
#ifdef MAPLEROOT_SINGLE_DLL
#include <detours.h> // the Detours 4 build slums.dll links once
#else
#include "detours.h"
#pragma comment(lib, "detours.lib")
#endif
#include "INIReader.h"
#pragma comment(lib, "ws2_32.lib")

#define relative_address(frm, to) (int)(((int)to - (int)frm) - 5)

extern HMODULE _this;
extern int expTable[200];