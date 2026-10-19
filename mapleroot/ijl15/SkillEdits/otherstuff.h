#include "../Global.h"
#pragma once

auto  SetMovePathAttribute = (void(__thiscall*)(void*, int))0x0052EF17;

__declspec(naked) void getUpdateTime() {
	__asm {
		mov eax, 0x00BE7B38
		mov eax, [eax + 18]
		retn
	}
}

auto CanSendExclRequest = (int(__thiscall*)(void*, int, int))0x00485BF7;
int(__fastcall CanSendExclRequest_t(void* _this, int a2, int a3))
{
	return 1;
}

auto get_cool_time = (int(__cdecl*)(int nSkillID))0x009535E3;
int(__cdecl get_cool_time_t)(int nSkillID)
{
	
}