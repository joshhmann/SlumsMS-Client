#include "../Global.h"


int SkillEntryOut;
auto is_keydown_skill = (int(__cdecl*)(int))0x004FB08F;
int(__cdecl is_keydown_skill_t)(int nSkillID)
{
	if (nSkillID == 1201016 || nSkillID == 1201013)
	{
		return 1;
	}
	else 
	{
		return 0;
	}
}

auto IsImmovable = (int(__thiscall*)(void*))0x0095F914;
int(__fastcall isImmovable_t)(void* _this, void* edx)
{
	return 0;
}

auto doMeleeAttack = (int(__thiscall*)(void*, int, int, void*, int, int, int, int))0x00950921;
int(__fastcall tryDoingMeleeAtack(void* _this, void* edx, int a2, int a3, void* fuckall, int a5, int a6, int a7, int a8))
{
	int dma = doMeleeAttack(_this, a2, a3, fuckall, a5, a6, a7, a8);
	if (a2 == 0) 
	{
		
	}
	return dma;
}

