//#include "../Global.h"
//#include "../HackStuff/haxAddresses.h"
//#pragma once
//
//int spearmulti = 0x00AFE848;
//
//// addies
//int heavystrike1 = 0x0095145B;
//int attackspeed0 = 0x00765070;
//int attackspeed1 = 0x00453E42;
//int attackspeed2 = 0x00453E47;
//int spearMastery = 0x00764877;
//int pinaSpeed = 0x00765060;
//int brewingSpeed = 0x0096C0A7;
//int brandishCheck = 0x00950F7C;
//int attackspeedcalc = 0x00942AF8;
//int combat1 = 0x0096DABE;
//int combat2 = 0x0096DACE;
//int comba = 0x00967982;
//int spearover = 0x0078F3FB;
//int spearstab = 0x0078F4A8;
//int ohsw = 0x0078F84F;
//int thsw = 0x0078F555;
//int drawohsw1 = 0x008C2F86;
//int drawthsw1 = 0x008C2D70;
//int drawohsw = 0x008C2FCF;
//int drawthsw = 0x008C2DB9;
//int drawstab = 0x008C2CE3;
//int drawswing = 0x008C2D2C;
//int heavystrikee = 0x009679A6;
//int brandishjmp = 0x0095255A;
//int heavystrikej = 0x00933ABF;
//int dicrits = 0x007650AF;
//int thieffjjmpbck = 0x009680b8;
//int stancejmpbck = 0x00958ad1;
//int shoot = 0x009690E9;
//int stancefunc = 0x00958add;
//int fjjumpback1 = 0x0096bef8;
//int fjaddy1 = 0x0096bf52;
//int combataddy = 0x0096bF14;
//int focus = 0x00967f0d;
//int focuscall = 0x009691ac;
//int callbound = 0x0096897a;
//int brandishjmpback = 0x0095255a;
//int domelee = 0x0095262c;
//int domelee1 = 0x00950f74;
//int pVecCtrl = 0x0052EF17;
//int skilleffect = 0x00933990;
//int skillusereq = 0x0096D399;
//int boundjmpswordsman = 0x00967982;
//int jgloc1 = 0x009679D8; // bound jump
//int jzloc1 = 0x009691AC; // BUFF
//int reversestep = 0x0096DAA0;
//int isleft = 0x00416563;
//int combatstep = 0x00969026;
//int combatstepjback = 0x0096799E;
//int sleep = 0x00953688;
//int sleepjmp = 0x00953605;
//int sleepback = 0x00953611;
//int powerstrikejmp = 0x00967991;
//int powerstrikejmpback = 0x00950DEA;
//int slashblast = 0x0095145B;
//int slashblast1 = 0x00952660;
//int slashblastback = 0x00951460;
//int slashback = 0x00935890;
//int slashjmpback = 0x00933AC5;
//int begskillback = 0x00933A6C;
//int onehandonlyjmp = 0x00950DE5;
//int onehandback = 0x00950DEA;
//int jeloc = 0x00950F74;
//int jlheavystrike = 0x0093587C;
//int fuckingidiot = 0x009689DF;
//int dumbjmpback = 0x009686EB;
//int brandishani = 0x0093462F;
//int brandishani2 = 0x00934810;
//int anijmpback = 0x0093463B;
//int statchange = 0x00967B71;
//int satchangejmp = 0x0096793B;
//
//int vitmultjmpback = 0x0078F7C2;
//void _declspec(naked) vitmult()
//{
//	_asm {
//		push dword ptr[esi + 0x38]
//		lea edi, [esi + 0x30]
//		jmp[vitmultjmpback]
//	}
//}
//
//int drawvitmultjmpback = 0x008C2FC0;
//void _declspec(naked) drawvitmult()
//{
//	_asm {
//		push dword ptr[esi + 0x2100]
//		mov dword ptr[ebp - 0x34], eax
//		lea eax, [esi + 0x20F8]
//		jmp[drawvitmultjmpback]
//	}
//}
//
//void _declspec(naked) drawoldvitmult()
//{
//	_asm {
//		push dword ptr[esi + 0x2100]
//		mov dword ptr[ebp - 0x38], eax
//		lea eax, [esi + 0x20f8]
//		jmp[drawvitmultjmpback]
//	}
//}
//
//void _declspec(naked) oldvitmult()
//{
//	_asm {
//		push dword ptr[esi + 0x2C]
//		lea edi, [esi + 0x24]
//		jmp[vitmultjmpback]
//	}
//}
//
//int critsjmp = 0x007650f5;
//void _declspec(naked) dCrits()
//{
//	_asm {
//		cmp eax, 50
//		pop ecx
//		jmp[critsjmp]
//	}
//}
//
//int yoinkjmpout = 0x00952360;
//int yoinkjmpback = 0x00952365;
//int yoincon = 0x00952367;
//
//int doactivejmpout = 0x0096792A;
//
//// jmpto addies
//int variable = 0;
//int combatStep = 0x00969026;
//int meleeAttack = 0x009690AE;
//int summonAttack = 0x009689DF;
//int prepareAttack = 0x00969229;
//int statChange = 0x00967B71;
//int doactivejmpback = 0x0096793B;
//int dorecovery = 0x00969217;
//
//
//
//void _declspec(naked)doYoink() {
//	_asm {
//		cmp eax, 1201013
//		je[var]
//		cmp eax, 1301012
//		je[var]
//		jmp[yoinkjmpback]
//
//		var: jmp[yoincon]
//	}
//}
//
//void _declspec(naked)doActiveSkills() {
//	_asm {
//		mov eax, 1050 // 2h
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1051 // 2h
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1052 // 2h
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1001055 // 2h
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1001071 // 2h
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1101010
//		cmp esi, eax
//		je[melee]
//		mov eax, 1101011
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1101012
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1101013
//		cmp esi, eax
//		je[melee]
//		mov eax, 1101014
//		cmp esi, eax
//		je[melee]
//		mov eax, 1101015
//		cmp esi, eax
//		je[combat]
//		mov eax, 1101016
//		cmp esi, eax
//		je[melee]
//		mov eax, 1101017
//		cmp esi, eax
//		je[melee]
//		mov eax, 1201010
//		cmp esi, eax
//		je[melee]
//		mov eax, 1201011
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1201013
//		cmp esi, eax
//		je[prepare]
//		mov eax, 1201014
//		cmp esi, eax
//		je[melee]
//		mov eax, 1201015
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1201016
//		cmp esi, eax
//		je[melee]
//		mov eax, 1201017
//		cmp esi, eax
//		je[recover]
//		mov eax, 1201018
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1301011
//		cmp esi, eax
//		je[melee]
//		mov eax, 1301012
//		cmp esi, eax
//		je[melee]
//		mov eax, 1301013
//		cmp esi, eax
//		je[melee]
//		mov eax, 1301014
//		cmp esi, eax
//		je[lb1]
//		mov eax, 1301015
//		cmp esi, eax
//		je[melee]
//		mov eax, 1301016
//		cmp esi, eax
//		je[melee]
//		mov eax, 1301017
//		cmp esi, eax
//		je[melee]
//		mov eax, 1111021
//		cmp esi, eax
//		je[recover]
//		mov eax, 2301005
//		jmp[doactivejmpback]
//
//		melee: jmp[meleeAttack]
//		summons : jmp[summonAttack]
//		prepare : jmp[prepareAttack]
//		lb1 : jmp[statChange]
//		combat : jmp[combatStep]
//		recover : jmp[dorecovery]
//		shoot2 : jmp[shoot]
//	}
//}
////jmpback 
//int preparejmpback = 0x0095c01e;
//int preparejmpout = 0x0095c010;
//int trydomelee = 0x0095C11D;
//
//void _declspec(naked)doPrepareSkills() {
//	_asm {
//		cmp eax, 1201013
//		je[lb1]
//		cmp eax, 1201016
//		je[lb1]
//		jmp[preparejmpback]
//
//		lb1 : jmp[trydomelee]
//	}
//}
//
//void _declspec(naked)mobAutoAggro() {
//	_asm {
//		call dword ptr[cVecCtrlWorkUpdateActiveCall] //calls CVecCtrl::WorkUpdateActive()
//		push eax
//		mov edx, [UserLocalBase]
//		mov edx, [edx]
//		mov eax, [OFS_pID]
//		mov edx, [edx + eax]
//		mov edx, [edx + 0x8]
//		mov eax, [OFS_Aggro]
//		mov[esi + eax], edx //Aggro Offset (first cmp before CVecCtrl::ChaseTarget)
//		pop eax
//		jmp dword ptr[mobAutoAggroAddrRet]
//	}
//}
//
//
//
////void _declspec(naked)doActiveSkills()
////{
////	_asm {
////		mov eax, 1050 // 2h
////		cmp esi, eax
////		je[lb1]
////		mov eax, 1051 // 2h
////		cmp esi, eax
////		je[lb1]
////		mov eax, 1052 // 2h
////		cmp esi, eax
////		je[lb1]
////		mov eax, 2301005
////		jmp[satchangejmp]
////		lb1: jmp[statchange]
////	}
////}
//
//void __declspec(naked) MouseFlyX() {
//	_asm {
//		push eax
//		push ecx
//		mov eax, [UserLocalBase]
//		mov eax, [eax]
//		mov ecx, [OFS_pID]
//		mov eax, [eax + ecx]
//		cmp esi, eax
//		pop eax
//		jne ReturnX
//		mov eax, [InputBase]
//		mov eax, [eax]
//		mov ecx, [OFS_MouseLocation]
//		mov eax, [eax + ecx]
//		mov ecx, [OFS_MouseX]
//		mov eax, [eax + ecx]
//
//		ReturnX:
//		pop ecx
//			mov[ebx], eax
//			mov edi, [ebp + 0x10]
//			jmp dword ptr[mouseFlyXAddrRet]
//	}
//}
//
//void __declspec(naked) MouseFlyY() {
//	_asm {
//		push eax
//		push ecx
//		mov eax, [UserLocalBase]
//		mov eax, [eax]
//		mov ecx, [OFS_pID]
//		mov eax, [eax + ecx]
//		cmp esi, eax
//		pop eax
//		jne ReturnY
//		mov eax, [InputBase]
//		mov eax, [eax]
//		mov ecx, [OFS_MouseLocation]
//		mov eax, [eax + ecx]
//		mov ecx, [OFS_MouseY]
//		mov eax, [eax + ecx]
//
//		ReturnY:
//		pop ecx
//			mov[edi], eax
//			mov ebx, [ebp + 0x14]
//			jmp dword ptr[mouseFlyYAddrRet]
//	}
//}
//
//
//int lilysolutionjmpback = 0x009F71D0;
//void __declspec(naked) lilySolution() {
//	_asm {
//		push 5000
//		push 5000
//		push 17
//		push esi
//		jmp dword ptr[lilysolutionjmpback]
//	}
//}
//
//
//void _declspec(naked) ReverseStep() {
//	_asm {
//		mov eax, 0x10CCD0
//		cmp esi, eax
//		jg[lb1]
//		je[lb2]
//		mov eax, 0x10CCCF
//		cmp esi, eax
//		je[lb3]
//		mov eax, 0xF462B
//		cmp esi, eax
//		je[lb3]
//		jmp[combatstepjback]
//
//		lb1:
//		jmp[jgloc1]
//
//			lb2 :
//			jmp[combatstep]
//
//			lb3 :
//			jmp[jzloc1]
//
//	}
//}
//
//
//void _declspec(naked) cSleepTime() {
//	_asm {
//		cmp eax, 0x10CCD0
//		je[lb1]
//		cmp eax, 0x111AE9
//		jmp[sleepback]
//
//		lb1:
//		jmp[sleep]
//	}
//}
//
//
//int enableback = 0x008C58B7;
//void _declspec(naked)enableAP() {
//	_asm {
//		push 1
//		call[eax + 0x1C]
//		add ebx, 8
//		jmp[enableback]
//	}
//}
//// 8c4e27
////
//
//
//int detailDraw = 0x008C4E27;
//void _declspec(naked)detailDraws() {
//	_asm {
//		push 154
//		push 2006
//		jmp[detailDraw]
//	}
//}
//
//int apReset = 0x008C7B27;
//void _declspec(naked)apResets() {
//	_asm {
//		push 212
//		push edi
//		push 2000
//		jmp[apReset]
//	}
//}
//
//
//int apDraw = 0x008C6B6C;
//void _declspec(naked)apDraws() {
//	_asm {
//		push 208
//		push 50
//		jmp[apDraw]
//	}
//}
//
//int fasttextJmpback = 0x09A4F12;
//void _declspec(naked)fast_text() {
//	_asm {
//
//		add[esi + 0x6D4], 1234567
//		jmp[fasttextJmpback]
//	}
//}
//
//int clientthing1jmpback = 0x055BEEF;
//void _declspec(naked)clientthing1() {
//	_asm {
//		add edx, 0x01DD
//		add ecx, 0xAA
//		jmp[clientthing1jmpback]
//	}
//}
//
//int clientthing2jmpback = 0x055C08B;
//void _declspec(naked)clientthing2() {
//	_asm {
//		add eax, 0xAA
//		push eax
//		mov eax, [ebp - 0x3C]
//		add eax, 0x01DD
//		jmp clientthing2jmpback
//	}
//}
//
//void clientHacks()
//{
//	//PatchJmp(0x0092DE9F, (void*)ASTEST, 0);
//	// change equipment limit
//	WriteByte(0x004F1024, 0xEB);
//	WriteByte(0x004F135A, 0xEB);
//	WriteByte(0x004F1E42, 0xEB);
//	// change stat limit
//	WriteByte(0x00A239EE, 0xEB);
//	WriteByte(0x00A23B74, 0xEB);
//	//lacking skill
//	WriteByte(0x008AD163, 0xEB);
//	WriteByte(0x008AD21A, 0xEB);
//	WriteByte(0x008AD0CA, 0xEB);
//
//	//prepareskill not stopped
//	WriteByte(0x009592E7, 0xEB);
//
//	CodeCave((void*)clientthing1, 0x0055BEE6, 0);
//	CodeCave((void*)clientthing2, 0x0055C07F, 0);
//
//	CodeCave((void*)lilySolution, 0x009F71CB, 0);
//	//CodeCave((void*)fast_text, fasttextJmpback, 0);
//	// MIGHT BE TO JMP
//	PatchNop(0x00A20006, 2);
//	// genderless
//	PatchNop(0x00460AED, 2);
//	//pdd
//	PatchNop(0x0079318E, 9);
//	PatchJmp(0x00793111);
//	WriteByte(0x00793111 + 1, 0x41);
//	WriteByte(0x00793111 + 2, 0x03);
//	WriteByte(0x00793111 + 3, 0x00);
//	WriteByte(0x00793111 + 4, 0x00);
//	WriteValue(0x00793129 + 2, 0x00AF0DE0);
//	WriteValue(0x00793135 + 2, 0x00AF0DE0);
//	WriteValue(0x00793331 + 2, 0x00AFE838);
//	WriteValue(0x0079330C + 2, 0x00AFE838);
//	WriteValue(0x0079320A + 2, 0x00AFE838);
//	WriteValue(0x00793213 + 2, 0x00AFE838);
//	WriteValue(0x0079321E + 2, 0x00AFE838);
//	//mdd
//	PatchNop(0x00793520, 3);
//	PatchNop(0x00793525, 6);
//	PatchJmp(0x007934A3);
//	WriteByte(0x007934A3 + 1, 0xD8);
//	WriteByte(0x007934A3 + 2, 0x01);
//	WriteByte(0x007934A3 + 3, 0x00);
//	WriteByte(0x007934A3 + 4, 0x00);
//	WriteValue(0x0079358B + 2, 0x00AFE838);
//	WriteValue(0x00793594 + 2, 0x00AFE838);
//	WriteValue(0x007935F2 + 2, 0x00AFE838);
//	WriteValue(0x0079359F + 2, 0x00AFE838);
//	WriteValue(0x007934BB + 2, 0x00AF0DE0);
//	WriteValue(0x007934C7 + 2, 0x00AF0DE0);
//	// MOB MAXSTAT
//	WriteValue(0x0067DD1D + 1, 999999);
//	WriteValue(0x00793499 + 1, 999999);
//	WriteValue(0x00793107 + 1, 999999);
//	WriteValue(0x007926DD + 1, 999999);
//	WriteValue(0x0077E215 + 1, 999999);
//	WriteValue(0x00780620 + 1, 999999);
//	// f1 fly
//	//WriteByte(0x0095099A, 0xEB);
//	//WriteByte(0x009509DC, 0xEB);
//	//WriteByte(0x0095385B, 0xEB);
//	//WriteByte(0x00955783, 0xEB);
//	//WriteByte(0x0095F161, 0xEB);
//	//WriteByte(0x0095F1A3, 0xEB);
//	//WriteByte(0x009571BB, 0xEB);
//	//WriteByte(0x009571F6, 0xEB);
//	// speed
//	//WriteValue(0x00780743 + 3, 3000);
//	//WriteValue(0x008C4286 + 1, 3000);
//	//// partywindow kill
//	//PatchNop(0x005241B3, 1);
//	//PatchNop(0x005241E2, 1);
//	//// tubi
//	//PatchNop(0x00485C08, 2);
//	// someLevel dx8
//}
//
////int __thiscall CUserPreview::DoMeleeAttack(int* , int , signed int)+
//
//
//void skillhacks() {
//	//PatchNop(0x00453E37, 34); // action speed caps
//	CodeCave((void*)doActiveSkills, doactivejmpout, 5);
//	CodeCave((void*)doPrepareSkills, preparejmpout, 4);
//	//PatchNop(brandishCheck, 15); // 1-h swordonly
//	PatchNop(spearMastery, 10); // all spears use mastery
//
//	PatchNop(combat1, 2); //
//
//	PatchNegEax(combat2);
//
//	WriteByte(0x008ECB02, 0xEB); // je to jmp draw tooltip attack speed
//
//	WriteByte(0x00766239, 0x1E); // knuckle to 1h
//
//	WriteByte(0x00969567, 0x7E);
//
//	WriteByte(0x00969568, 0xA3);
//
//	PatchNop(0x00969569, 4);
//
//	WriteValue(0x00952E1F + 3, 1301015);
//
//	WriteValue(0x00952114 + 1, 1301015);
//
//	WriteValue(0x00950B69 + 1, 1301015);
//	// GET ME TO MIST
//	WriteByte(0x00431D44, 0xE9);
//	WriteByte(0x00431D45, 0xA6);
//	WriteByte(0x00431D46, 0x01);
//	WriteByte(0x00431D47, 0x00);
//	WriteByte(0x00431D48, 0x00);
//	WriteByte(0x00431D49, 0x90);
//	//rushskill end
//
//	PatchNop(0x00952E2F, 6); // rush no move skill
//
//	//infinite spear start
//	WriteValue(0x00969502, 1301012);
//	WriteByte(0x00969506, 0x0F);
//	WriteByte(0x00969507, 0x84);
//	WriteByte(0x00969508, 0x17);
//	WriteByte(0x00969509, 0x01);
//
//
//	WriteValue(0x009521F5, 1101013);
//
//	//endcombo
//
//	/*WriteValue(0x0078DEBA + 1, 1101016);*/
//
//	//WriteValue(0x0096CE4D, 1301014); // spark stuff
//
//	//WriteValue(0x009548D5 + 2, 1301014);
//
//	CodeCave((void*)doYoink, yoinkjmpout, 0);
//
//
//
//	WriteValue(0x00764884 + 1, 42); //spear mastery
//	WriteValue(0x0076483A + 1, 40); // 2-hsword
//	WriteValue(0x00764855 + 1, 41); // 1h- sword
//	WriteValue(0x0095CE0A, 0xEB); // Achilles jmp
//	//WriteValue(0x00766632 + 1, 1101016);
//	PatchNop(0x007A5610, 2); // SUMMONS ARE MAGIC
//
//	WriteValue(0x0095CE0B, 0x1F);
//
//	WriteValue(0x0095ce2c, 61); // vitality da 
//
//	PatchNop(0x008C2A94, 6); //ui draw mastery;
//
//	PatchNop(0x0078E0EA, 6); // calc damage mastery
//
//	PatchNop(0x00935894, 2);
//
//	PatchNop(0x00761823, 2); // max skill level
//
//	//CodeCave((void*)BrandishActives, 0x009686DF, 7);
//
//	//CodeCave((void*)BrandishAni, brandishani, 1); // new heavystrike
// // CRITSTAT
//
//	PatchNop(0x007650F9, 6); //CRITSTUFF?
//}
////ui stuff
//
////### Readme:
////### ARRAYS - These go at the top of the file, or in a header or somewhere. Not inside Client::Functions
////### CLIENT EDITS - These go inside a function in the client class. ("Client::UpdateGame", etc.)
////### CODECAVES CLIENT EDITS - Same as Client Edits
////### CODECAVES - In a header or somewhere, as long as they are "#include#" in the file you call them from. 
////### Enjoy, have fun, be nice, report bugs. 
//
//// ARRAYS ----
//unsigned char Array_aDefaultQKM[] = {
//	42, 0, 0, 0,
//	82, 0, 0, 0,
//	71, 0, 0, 0,
//	73, 0, 0, 0,
//	2, 0, 0, 0,
//	3, 0, 0, 0,
//	4, 0, 0, 0,
//	5, 0, 0, 0,
//	6, 0, 0, 0,
//	30, 0, 0, 0,
//	31, 0, 0, 0,
//	32, 0, 0, 0,
//	33, 0, 0, 0,
//	29, 0, 0, 0,
//	83, 0, 0, 0,
//	79, 0, 0, 0,
//	81, 0, 0, 0,
//	16, 0, 0, 0,
//	17, 0, 0, 0,
//	18, 0, 0, 0,
//	19, 0, 0, 0,
//	20, 0, 0, 0,
//	44, 0, 0, 0,
//	45, 0, 0, 0,
//	46, 0, 0, 0,
//	47, 0, 0, 0,
//	52, 0, 0, 0
//};
//
//// 0x00BE2DB0 confirmed, s_ptShortKeyPos
//unsigned char Array_ptShortKeyPos[] = {
//	7, 0, 0, 0,
//	8, 0, 0, 0,
//	42, 0, 0, 0,
//	8, 0, 0, 0,
//	77, 0, 0, 0,
//	8, 0, 0, 0,
//	112, 0, 0, 0,
//	8, 0, 0, 0,
//	147, 0, 0, 0,
//	8, 0, 0, 0,
//	182, 0, 0, 0,
//	8, 0, 0, 0,
//	217, 0, 0, 0,
//	8, 0, 0, 0,
//	252, 0, 0, 0,
//	8, 0, 0, 0,
//	287, 1, 0, 0,
//	8, 0, 0, 0,
//	322, 1, 0, 0,
//	8, 0, 0, 0,
//	357, 1, 0, 0,
//	8, 0, 0, 0,
//	392, 1, 0, 0,
//	8, 0, 0, 0,
//	427, 1, 0, 0,
//	8, 0, 0, 0,
//	7, 0, 0, 0,
//	41, 0, 0, 0,
//	42, 0, 0, 0,
//	41, 0, 0, 0,
//	77, 0, 0, 0,
//	41, 0, 0, 0,
//	112, 0, 0, 0,
//	41, 0, 0, 0,
//	147, 0, 0, 0,
//	41, 0, 0, 0,
//	182, 0, 0, 0,
//	41, 0, 0, 0,
//	217, 0, 0, 0,
//	41, 0, 0, 0,
//	252, 0, 0, 0,
//	41, 0, 0, 0,
//	287, 1, 0, 0,
//	41, 0, 0, 0,
//	322, 1, 0, 0,
//	41, 0, 0, 0,
//	357, 1, 0, 0,
//	41, 0, 0, 0,
//	392, 1, 0, 0,
//	41, 0, 0, 0,
//	427, 1, 0, 0,
//	41, 0, 0, 0
//};
////Variant of Array_ptShortKeyPos
//unsigned char Array_ptShortKeyPos_Fixed_Tooltips[] = {
//	7,0,0,0,0,0,0,0,42,0,0,0,0,0,0,0,77,0,0,0,0,0,0,0,112,0,0,0,0,0,0,0,147,0,0,0,0,0,0,0,182,0,0,0,0,0,0,0,217,0,0,0,0,0,0,0,252,0,0,0,0,0,0,0,287,1,0,0,0,0,0,0,322,1,0,0,0,0,0,0,357,1,0,0,0,0,0,0,392,1,0,0,0,0,0,0,427,1,0,0,0,0,0,0,7,0,0,0,33,0,0,0,42,0,0,0,33,0,0,0,77,0,0,0,33,0,0,0,112,0,0,0,33,0,0,0,147,0,0,0,33,0,0,0,182,0,0,0,33,0,0,0,217,0,0,0,33,0,0,0,252,0,0,0,33,0,0,0,287,1,0,0,33,0,0,0,322,1,0,0,33,0,0,0,357,1,0,0,33,0,0,0,392,1,0,0,33,0,0,0,427,1,0,0,33,0,0,0
//};// This array will fix the janky offset of the tooltips
//// s_aDefaultQKM_0
//unsigned char Array_aDefaultQKM_0[] = {
//	42, 0, 0, 0,
//	82, 0, 0, 0,
//	71, 0, 0, 0,
//	73, 0, 0, 0, //4
//	29, 0, 0, 0,
//	83, 0, 0, 0,
//	79, 0, 0, 0,
//	81, 0, 0, 0, //8
//	42, 0, 0, 0,
//	82, 0, 0, 0,
//	71, 0, 0, 0,
//	73, 0, 0, 0, //12
//	29, 0, 0, 0,
//	83, 0, 0, 0,
//	79, 0, 0, 0,
//	81, 0, 0, 0, //16
//	84, 0, 0, 0,
//	85, 0, 0, 0,
//	86, 0, 0, 0,
//	87, 0, 0, 0, //20 
//	88, 0, 0, 0,
//	89, 0, 0, 0,
//	29, 0, 0, 0,
//	29, 0, 0, 0, //24
//	29, 0, 0, 0,
//	29, 0, 0, 0,
//	29, 0, 0, 0,
//};
//
//unsigned char Array_Expanded[312] = { 4, 4, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 0, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 1, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 2, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 3, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 5, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 6, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 7, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 8, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 10, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 11, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 12, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 13, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 14, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 15, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 16, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 17, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 23, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 24, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 25, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 26, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	4, 27, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	5, 50, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	5, 51, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	5, 52, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0,
//	5, 53, 0, 0,
//	0, 0, 0, 0,
//	0, 0, 0, 0 };
//
//unsigned char Array_Expanded_Testing_Cooldown_fix[312] = { 0 };
//
//unsigned char cooldown_Array[124] = { 255, 255, 255, 255, 255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255 };
//
//// CODECAVES --- 
//DWORD Array_aDefaultQKM_Address = (DWORD)&Array_aDefaultQKM;
//DWORD Array_mystery_Address = (DWORD)&Array_Expanded;
//DWORD Array_mystery_Address_plus = (DWORD)&Array_Expanded + 1;
//DWORD cooldown_Array_Address = (DWORD)&cooldown_Array;
//DWORD Array_Expanded_Testing_Cooldown_fix_Address = (DWORD)&Array_Expanded_Testing_Cooldown_fix;
//
//DWORD CompareValidate_Retn = 0x8DD8BD;
//_declspec(naked) void CompareValidateFuncKeyMappedInfo_cave()
//{
//	_asm
//	{
//		push 0x138;
//		push 0x0;
//		push eax;
//		pushad;
//		popad;
//		jmp CompareValidate_Retn
//			//push 0x8DD8BD;
//			//ret;
//	}
//}
//
//
//DWORD sub_9FA0CB_cave_retn_1 = 0x9FA0E1;
//_declspec(naked) void sub_9FA0CB_cave()
//{
//	_asm {
//		test eax, eax;
//		jne label;
//		push 0xD4;
//		pushad;
//		popad;
//		// -> ZAllocEx<ZAllocAnonSelector>::Alloc(ZAllocEx<ZAllocAnonSelector>::_s_alloc, 0x44u);
//		//push 0x9FA0E1;
//		//ret;
//		jmp sub_9FA0CB_cave_retn_1
//			label :
//		push 0x138;
//		push 0x0;
//		push eax;
//		pushad;
//		popad;
//		// -> memset(this + 0xD20, 0, 0x60u);
//		//push 0x8DD8BD;
//		//ret;
//		jmp CompareValidate_Retn
//	}
//}
////DWORD sDefaultQuickslotKeyMap_cave_retn = 0x72B7C2;
//_declspec(naked) void sDefaultQuickslotKeyMap_cave()
//{
//	_asm {
//		push ebx;
//		push esi;
//		push edi;
//		xor edx, edx;
//		mov ebx, ecx;
//		call label;
//		nop;
//		lea edi, dword ptr ds : [ebx + 0x4] ;
//		mov ecx, 0x1A;
//		mov esi, Array_aDefaultQKM_Address;
//		rep movsd;
//		lea edi, dword ptr ds : [ebx + 0x6C] ;
//		mov ecx, 0x1A;
//		mov esi, Array_aDefaultQKM_Address;
//		rep movsd;
//		pop edi;
//		pop esi;
//		pop ebx;
//		ret;
//		// 0xBF8EE8
//	label:
//		push esi;
//		mov esi, ecx;
//		lea eax, dword ptr ds : [esi + 0x4] ;
//		// -> _DWORD *__fastcall sub_72B7BC(_DWORD *a1)
//		push 0x72B7C2;
//		ret;
//		//jmp sDefaultQuickslotKeyMap_cave_retn
//	}
//}
//_declspec(naked) void DefaultQuickslotKeyMap_cave()
//{
//	_asm {
//		push esi;
//		push edi;
//		lea eax, dword ptr ds : [ecx + 0x4] ;
//		mov esi, Array_aDefaultQKM_Address;
//		mov ecx, 0x1A;
//		mov edi, eax;
//		rep movsd;
//		pop edi;
//		pop esi;
//		ret;
//	}
//}
//_declspec(naked) void Restore_Array_Expanded() //Thank you Max
//{
//	_asm {
//		lea eax, [esi + 0D7Ch]
//		push esi
//		push edi
//		push ecx
//		mov esi, [Array_Expanded_Testing_Cooldown_fix_Address]
//		mov edi, Array_mystery_Address
//		mov ecx, 78
//		rep movsd
//		pop ecx
//		pop edi
//		pop esi
//		push 0x008CFE03;
//		ret;
//	}
//}
//
//void darnell() {
//	// CLIENT EDITS ----
//		// CUIStatusBar::OnCreate
//	WriteByte(0x008D155C + 1, 0xF0); // Draw rest of quickslot bar
//	WriteByte(0x008D155C + 2, 0x03);
//	WriteByte(0x008D182E + 1, 0xF0); // Draw rest of hotkeys
//	WriteByte(0x008D182E + 2, 0x03);
//	WriteByte(0x008D1AC0 + 1, 0xF0); // Draw rest of cooldowns, who tf knows why
//	WriteByte(0x008D1AC0 + 2, 0x03);
//
//	//----CQuickslotKeyMappedMan::CQuickslotKeyMappedMan?????
//	WriteValue(0x0072B7CE + 1, (DWORD)&Array_aDefaultQKM_0);
//	WriteValue(0x0072B8EB + 1, (DWORD)&Array_aDefaultQKM_0);
//
//	//----CUIStatusBar::CQuickSlot::CompareValidateFuncKeyMappedInfo
//	WriteByte(0x008DD916, 0x1A); // increase 8 --> 26
//	WriteByte(0x008DD8AD, 0x1A); // increase 8 --> 26
//	WriteByte(0x008DD8FD, 0xBB);
//	WriteValue(0x008DD8FD + 1, (DWORD)&Array_Expanded);
//	WriteByte(0x008DD8FD + 5, 0x90); //Errant byte
//	WriteByte(0x008DD898, 0xB8);
//	WriteValue(0x008DD898 + 1, (DWORD)&Array_Expanded);
//	WriteByte(0x008DD898 + 5, 0x90); //Errant Byte
//
//	//----CUIStatusBar::CQuickSlot::Draw
//	WriteByte(0x008DE75E + 3, 0x6C);
//	WriteByte(0x008DDF99, 0xB8);
//	WriteValue(0x008DDF99 + 1, (DWORD)&Array_Expanded);
//	PatchNop(0x008DDF99 + 5, 3); // Nopping errant operations
//
//	//----CUIStatusBar::OnMouseMove
//	WriteByte(0x008D7F1E + 1, 0x34);
//	WriteByte(0x008D7F1E + 2, 0x85);
//	WriteValue(0x008D7F1E + 3, (DWORD)&Array_Expanded);
//
//	//----CUIStatusBar::CQuickSlot::GetPosByIndex
//	WriteValue(0x008DE94D + 2, (DWORD)&Array_ptShortKeyPos);
//	WriteValue(0x008DE955 + 2, (DWORD)&Array_ptShortKeyPos + 4);
//	WriteByte(0x008DE941 + 2, 0x1A); //change cmp 8 --> cmp 26
//
//	//CUIStatusBar::GetShortCutIndexByPos
//	WriteValue(0x008DE8F4 + 1, (DWORD)&Array_ptShortKeyPos_Fixed_Tooltips + 4);
//	WriteByte(0x008DE926 + 1, 0x3E);
//
//	//CUIStatusBar::CQuickSlot::DrawSkillCooltime
//	WriteByte(0x008E099F + 3, 0x1A);
//	WriteByte(0x008E069D, 0xBE);
//	WriteValue(0x008E069D + 1, (DWORD)&cooldown_Array); //Pass enlarged FFFFF array
//	WriteByte(0x008E069D + 5, 0x90); //Errant byte
//	WriteByte(0x008E06A3, 0xBF);
//	WriteValue(0x008E06A3 + 1, (DWORD)&Array_Expanded + 1);
//	WriteByte(0x008E06A3 + 5, 0x90);
//
//	//----CDraggableMenu::OnDropped
//	WriteByte(0x004F928A + 2, 0x1A); //change cmp 8 --> cmp 26
//	//----CDraggableMenu::MapFuncKey
//	WriteByte(0x004F93F9 + 2, 0x1A); //change cmp 8 --> cmp 26
//	//----CUIKeyConfig::OnDestroy
//	WriteByte(0x00833797 + 2, 0x6C); // Updates the offset to 108 (triple) (old->24h)
//	WriteByte(0x00833841 + 2, 0x6C); // Updates the offset to 108 (triple) (old->24h)
//	WriteByte(0x00833791 + 1, 0x68); // push 68h (triple)
//	WriteByte(0x0083383B + 1, 0x68); // push 68h (triple)
//	//----CUIKeyConfig::~CUIKeyConfig
//	WriteByte(0x0083287F + 2, 0x6C); // triple the base value at this hex (old->24h)
//	WriteByte(0x00832882 + 1, 0x68); // push 68h (triple)
//	//----CQuickslotKeyMappedMan::SaveQuickslotKeyMap
//	WriteByte(0x0072B8C0 + 2, 0x6C); // triple the base value at this hex (old->24h)
//	WriteByte(0x0072B8A0 + 1, 0x68); // push 68h, (triple) //CQuickslotKeyMappedMan::SaveQuickslotKeyMap
//	WriteByte(0x0072B8BD + 1, 0x68); // push 68h, (triple) //CQuickslotKeyMappedMan::SaveQuickslotKeyMap
//	//----CQuickslotKeyMappedMan::OnInit
//	WriteByte(0x0072B861 + 1, 0x68); // push 68h (triple) (these ones might have to be just 60)
//	WriteByte(0x0072B867 + 2, 0x6C); // triple the base value at this hex (old->24h)
//	//----CUIKeyConfig::CNoticeDlg::OnChildNotify????
//	WriteByte(0x00836A1E + 1, 0x68); // push 68h (triple)
//	WriteByte(0x00836A21 + 2, 0x6C); // triple the base value at this hex (old->24h)
//
//
//	// CODECAVES CLIENT EDITS ---- 
//	CodeCave(CompareValidateFuncKeyMappedInfo_cave, 0x8DD8B8, 5);
//	CodeCave(sub_9FA0CB_cave, 0x9FA0DB, 5);
//	CodeCave(sDefaultQuickslotKeyMap_cave, 0x72B7BC, 5);
//	CodeCave(DefaultQuickslotKeyMap_cave, 0x72B8E6, 5);
//	CodeCave(Restore_Array_Expanded, 0x008CFDFD, 6); //restores the skill array to 0s
//}
//
//
//const DWORD dwTempStatIconVPos = 0x007B2C97;
//const DWORD dwTempStatIconHpos = 0x007B2CB5;
//const DWORD dwTempStatCoolTimeVPos = 0x007B2DA0;
//const DWORD dwTempStatCoolTimeHPos = 0x007B2DBE;
//
//void ui_hacks() {
//	//bypass level 20 ap laws
//	WriteByte(0x008C58DD, 0x80);
//	WriteByte(0x008C58DE, 0xFB);
//	WriteByte(0x008C58DF, 0x01);
//	WriteByte(0x00a23a9f, 0xEB);
//	PatchNop(0x00A23A7D, 6);
//	PatchNop(0x00A23A4F, 6);
//	//end bypass
//	WriteValue(0x009CC6F9 + 2, 0x00C1CF80); //switch addy
//	WriteDouble(0x00C1CF80, 4.00); //Addy speed control
//	CodeCave((void*)apDraws, 0x008C6B65, 2);
//
//	CodeCave((void*)detailDraws, 0x008C4E20, 2);
//
//	CodeCave((void*)apResets, 0x008C7B1F, 3);
//
//	CodeCave((void*)enableAP, 0x008C58B0, 2);
//
//	PatchNop(0x0062EE59, 16);
//
//	WriteValue(0x008D29B5, 701);
//
//	WriteValue(0x008d276b, 218);
//
//	WriteValue(0x008d2766, 701);
//
//	WriteValue(0x008D29BA, 433);
//
//	WriteByte(0x00452316, 0x7C);
//
//	WriteValue(0x008c7cec, 1000000); // autoassign
//
//	WriteValue(0x008C7B75, 1000000); //mp assign
//
//	WriteValue(drawstab + 2, 0x00AF3728); // draw damage change
//
//	WriteValue(drawswing + 2, 0x00AF3728); // draw damagechange
//
//	WriteValue(drawstab + 2, 0x00AFE868); // draw damage change
//
//	WriteValue(drawswing + 2, 0x00AFE868); // draw damagechange
//
//	WriteValue(drawthsw + 2, 0x00AFE868); // draw damage change
//
//	WriteValue(drawohsw1 + 2, 0x00AFE868); // draw damagechange
//
//	WriteValue(dwTempStatCoolTimeVPos + 2, -257);
//	WriteValue(dwTempStatIconVPos + 2, -257);
//	WriteValue(dwTempStatCoolTimeHPos + 3, 257);
//	WriteValue(dwTempStatIconHpos + 3, 257);
//}
////end ui
//void misc_hacks() {
//	CodeCave((void*)dCrits, dicrits, 5); // critanywhere
//
//
//	CodeCave((void*)cSleepTime, sleepjmp, 0);
//
//	WriteValue(spearover + 2, 0x00AFE868); // damagecalc 4.0
//
//	WriteValue(spearstab + 2, 0x00AFE868); // damagecalc 4.0
//
//	WriteValue(ohsw + 2, 0x00AFE868); // damagecalc 4.0
//
//	WriteValue(thsw + 2, 0x00AFE868); // damagecalc 4.0
//
//	WriteValue(drawthsw1 + 2, 0x00AFE868); // damagecalc 4.0
//
//	WriteValue(drawohsw + 2, 0x00AFE868); // damagecalc 4.0
//
//	CodeCave((void*)ReverseStep, comba, 23); // kitestep
//
//	// WriteValue(0x0096C0A7 + 1, 10);
//	//int fasttextaddr = 0x09A4F0C;
//	//PatchJmp(fasttextaddr, (void*)fast_text, 1);
//}
//
//
//
