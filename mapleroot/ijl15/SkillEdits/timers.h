#include "../HackStuff/haxAddresses.h"
using namespace std;
using chrono::duration_cast;
using chrono::milliseconds;
using chrono::system_clock;
//ints 
int Sus;
int SLevel;
int OLevel;
int TLevel;
int Spear;
int OSword;
int TSword;
int Crit;
int Defenses;
int TBLevel;
int FSLevel;
int BRLevel;
int STLevel;
int SRALevel;
int TankLevel;
int StasisLevel;
int PierceLevel;
int anicancelpro;
int guildbreath;
int futuresight;
int asPlus = 0;
int aniFrames = 0;
int startcanceltimer = 0;
int stasiscanceltimer = 120000;
int endcanceltimer = 0;
int cooldown = 0;
int lastUse = 0;
int iframeLevel = 0;
int pina;
int orbnum = 0;
int orbconsume = 0;
int skillAttackId = 0;
int darkenergy = 0;
int darkenergypro = 0;
int anicanceltimer = 0;
int aggropull = 0;
int storedDirection;



//bools
bool donormalattack = false;
bool greatCrash = false;
bool stasisRunning = false;
bool stasiscdrunning = false;
bool animationCancel = false;
bool onCD = false;
bool fsCancel = false;
bool tbCancel = false;
bool brCancel = false;
bool stCancel = false;
bool usingSRA = false;
bool canbeCancelled = false;
bool timerRunning = false;
bool cdTimerRunning = false;
bool macrocheck = false;
bool attackcheck = false;
bool inStasis = false;
bool tankmode = false;
bool glassmode = false;
bool energytimer = false;
bool onload = false;
bool flyhack = false;
bool ffsTimer = false;
bool aggroRunning = false;
bool aggrocdRunning = false;
bool fsRunning = false;
bool FutureSight = false;
bool combocheck = false;
bool tankCheck = false;
bool hurricane = false;
bool flipX = false;
bool jumping = false;
bool wtfnot = false;

chrono::time_point<chrono::steady_clock> start;
chrono::time_point<chrono::steady_clock> CDstart;
chrono::time_point<chrono::steady_clock> StasisCD;
chrono::time_point<chrono::steady_clock> StasisCDTimer;
chrono::time_point<chrono::steady_clock> MacroCheck;
chrono::time_point<chrono::steady_clock> EnergyTimer;
chrono::time_point<chrono::steady_clock> FFSTimer;
chrono::time_point<chrono::steady_clock> AggroTimer;
chrono::time_point<chrono::steady_clock> AggroCDTimer;
chrono::time_point<chrono::steady_clock> FSTimer;
chrono::time_point<chrono::steady_clock> FSCDTimer;

bool energyCheck()
{
	if (energytimer)
	{
		auto elapsed = chrono::steady_clock::now() - EnergyTimer;
		if (elapsed > chrono::milliseconds(5000 - ((darkenergy * 100) + (darkenergypro * 500)))) {
			energytimer = false;
			return true;
		}
		return false;
	}
	return false;
}


void FSCheck()
{
	if (fsRunning)
	{
		auto elapsed = chrono::steady_clock::now() - FSTimer;
		if (elapsed > chrono::milliseconds(300000)) {
			fsRunning = false;
		}
	}
}


void FFSCheck()
{
	if (ffsTimer)
	{
		auto elapsed = chrono::steady_clock::now() - FFSTimer;
		if (elapsed > chrono::milliseconds(10000)) {
			ffsTimer = false;
		}
	}
}

void aggroCheck()
{
	if (aggroRunning)
	{
		auto elapsed = chrono::steady_clock::now() - AggroTimer;
		if (elapsed > chrono::milliseconds(10000))
		{
			aggroRunning = false;
			WriteByte(mobAutoAggroAddr, 0xE8);
			WriteByte(mobAutoAggroAddr + 1, 0xD4);
			WriteByte(mobAutoAggroAddr + 2, 0x4E);
			WriteByte(mobAutoAggroAddr + 3, 0xFF);
			WriteByte(mobAutoAggroAddr + 4, 0xFF);
		}
	}
}


void EnergyCooldown()
{
	using namespace std;
	orbnum += 1;
	EnergyTimer = chrono::steady_clock::now();
	energytimer = true;
}

void aggroTimerStart() {
	using namespace std;
	aggrocdRunning = true;
	aggroRunning = true;
	AggroTimer = chrono::steady_clock::now();
	AggroCDTimer = chrono::steady_clock::now();
}

//
void FSCooldown()
{
	using namespace std;
	FSCDTimer = chrono::steady_clock::now();
	FutureSight = true;
	fsRunning = true;
}

void timeCheck()
{
	if (timerRunning)
	{
		auto elapsed = chrono::steady_clock::now() - start;
		if (elapsed < chrono::milliseconds(startcanceltimer))
		{
			return;
		}
		else if (elapsed > chrono::milliseconds(startcanceltimer) && elapsed < chrono::milliseconds(endcanceltimer))
		{
			canbeCancelled = true;
			timerRunning = false;
		}
		else
		{
			timerRunning = false;
		}
	}
}

void aggroCDCheck()
{
	if (aggroRunning)
	{
		auto elapsed = chrono::steady_clock::now() - AggroCDTimer;
		if (elapsed > chrono::milliseconds(startcanceltimer) && elapsed < chrono::milliseconds(60000))
		{
			aggrocdRunning = false;
		}
	}
}

void cdCheck()
{
	if (cdTimerRunning)
	{
		auto elapsed = chrono::steady_clock::now() - CDstart;
		if (elapsed > chrono::milliseconds(cooldown)) {

			onCD = false;
			cdTimerRunning = false;
		}
	}
}


bool stasisCheck()
{
	if (stasisRunning)
	{
		auto elapsed = chrono::steady_clock::now() - StasisCD;
		if (elapsed > chrono::milliseconds(5000)) {
			stasisRunning = false;
			StasisCDTimer = chrono::steady_clock::now();
			inStasis = false;
			return false;
		}
		return true;
	}
	else {
		return false;
	}
}

bool stasisonCheck()
{
	if (stasiscdrunning)
	{
		auto elapsed = chrono::steady_clock::now() - StasisCDTimer;
		if (elapsed > chrono::milliseconds(stasiscanceltimer)) {
			stasiscdrunning = false;
			return false;
		}
		return true;
	}
	else {
		return false;
	}
}

//
void StasisCooldown()
{
	using namespace std;
	inStasis = true;
	StasisCD = chrono::steady_clock::now();
	stasisRunning = true;
}

void AniCancelTimer(int nskillID)
{
	using namespace std;
	if (nskillID == 1101013)
	{
		startcanceltimer = 300 - anicanceltimer;
		endcanceltimer = 900;
	}
	else if (nskillID == 1101014)
	{
		startcanceltimer = 300 - anicanceltimer;
		endcanceltimer = 900;
	}
	start = chrono::steady_clock::now();
	timerRunning = true;
}

void FfsTimer()
{
	ffsTimer = true;
	FFSTimer = chrono::steady_clock::now();
	tankmode = !tankmode;
}

void canNotResetTimer(int nSkillID)
{
	if (nSkillID == 1101013)
	{
		cooldown = 810 - anicanceltimer;
	}
	if (nSkillID == 1101016)
	{
		cooldown = 720 - anicanceltimer;
	}
	cdTimerRunning = true;
	CDstart = chrono::steady_clock::now();
	onCD = true;
}