#include "../Global.h"
#pragma once

auto ShowSkillEffect = (void(__thiscall*)(void*, void*, int, int, int, int, void*))0x00933990;
void(__fastcall ShowSkillEffect_t)(void* _this, void* ecx, void* pSkill, int nSLV, int nActionSpeed, int bLeft, int nLast, void* pPtOffset)
{
	return (void)ShowSkillEffect(_this, pSkill, nSLV, nActionSpeed, bLeft, nLast, pPtOffset);
}

auto SetAttackAction = (int(__thiscall*)(void*, int, int, void*, int))0x0092EDB2;
int __fastcall SetAttackAction_t(void* _this, void* edx, int nAttackAction, int nAttackSpeed, void* pSkill, int nSLV)
{
	//Log("ActionCode %7d", nAttackAction);
	return SetAttackAction(_this, nAttackAction, nAttackSpeed, pSkill, nSLV);
}

void Hook_SkillEffect(bool bEnable)
{
	SetHook(bEnable, reinterpret_cast<PVOID*>(&ShowSkillEffect), reinterpret_cast<PVOID>(&ShowSkillEffect_t));
}


void Hook_AttackAction(bool bEnable)
{
	SetHook(bEnable, reinterpret_cast<PVOID*>(&SetAttackAction), reinterpret_cast<PVOID>(&SetAttackAction_t));
}
