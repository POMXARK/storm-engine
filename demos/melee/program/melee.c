// Melee demo: a sword fight in the yard of the location demo. The sailor (the player) and a pirate
// hold sabers (BLADE models attached to the "saber_hand"/"saber_belt" labels of the fencer), the
// pirate is an NPC with the "Fight" task, the engine plays the attacks, blocks, parries and hit
// reactions and asks these scripts what they cost and what they do: life and energy live in the
// characters' attributes here. Over the pirate the location draws his life and energy bars.
// W/S walk, A/D or the mouse turn, Shift walks instead of running, E draws or sheathes the saber,
// F attacks fast, G attacks with force, Space holds a block, Q parries, R starts a new bout,
// Escape quits.

#define EXECUTE 0
#define REALIZE 1

#define MSG_LOCATION_ADD_MODEL 30100
#define MSG_LOCATION_SET_CHRS_PATCH 30120
#define MSG_LOCATION_MODELSPATH 30140
#define MSG_LOCATION_TEXTURESPATH 30141
#define MSG_CHARACTER_SETMODEL 30200
#define MSG_CHARACTER_TELEPORT_AY 30211
#define MSG_CHARACTER_SETBLADE 30360
#define MSG_CHARACTER_SETFTGLEVEL 30362
#define MSG_CHARACTER_EX_MSG 30373
#define MSG_NPCHARACTER_SETTASK 30400
#define MSG_CAMERA_SETTARGET 30500
#define MSG_CAMERA_FOLLOW 30510
#define LOG_ADD_STRING 45020
#define LOG_AND_ACTIONS_INIT 45022

// life and energy of a fencer; an action costs energy, it comes back by ENERGY_REGEN a second
#define MAX_HP 100.0
#define MAX_ENERGY 100.0
#define ENERGY_REGEN 12.0
#define TICK_MS 100

object Render;
object Scene;
object Weather;
object Yard;
object Groups;
object Sailor;
object Pirate;
object Camera;
object ScreenLog;

int shadowDensity[2];
int bout = 0;
bool boutOver = false;

void MapKey(string controlName, int keyCode)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
}

void Main()
{
	// key codes are Windows virtual keys, 256/257 are the mouse axes
	MapKey("ChrForward", 87);
	MapKey("ChrBackward", 83);
	MapKey("ChrTurnHL", 65);
	MapKey("ChrTurnHR", 68);
	MapKey("ChrRun", 16);
	MapKey("ChrTurnH", 256);
	MapKey("ChrCamTurnV", 257);
	// the fight controls the player character reads (player.cpp)
	MapKey("ChrFightMode", 69);
	MapKey("ChrAttackFast", 70);
	MapKey("ChrAttackForce", 71);
	MapKey("ChrBlock", 32);
	MapKey("ChrParry", 81);
	MapKey("Demo_Restart", 82);
	MapKey("Demo_Quit", 27);

	Render.BackColor = argb(255, 120, 160, 200);

	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);

	CreateEntity(&Weather, "weather");
	Weather.Fog.Enable = 1;
	Weather.Fog.Density = 0.02;
	Weather.Fog.Color = argb(0, 120, 160, 200);
	Weather.Sun.Color = argb(0, 255, 236, 200);
	Weather.Sun.Ambient = argb(0, 90, 100, 120);
	Weather.Sun.HeightAngle = 0.8;
	Weather.Sun.AzimuthAngle = 2.5;
	Weather.isDone = "";

	Yard.id = "yard";
	CreateEntity(&Yard, "location");
	SendMessage(&Yard, "ls", MSG_LOCATION_MODELSPATH, "yard");
	SendMessage(&Yard, "ls", MSG_LOCATION_TEXTURESPATH, "yard");
	if (!SendMessage(&Yard, "lssl", MSG_LOCATION_ADD_MODEL, "yard", "DLightModel", 10))
	{
		Trace("MELEE: yard model not loaded");
	}
	if (!SendMessage(&Yard, "ls", MSG_LOCATION_SET_CHRS_PATCH, "yard_patch"))
	{
		Trace("MELEE: character patch not loaded");
	}
	CreateEntity(&Groups, "CharactersGroups");

	// the event handlers go first: the characters ask them as soon as they are set up
	SetEventHandler("EWhr_GetShadowDensity", "Demo_ShadowDensity", 0);
	SetEventHandler("EWhr_GetFogDensity", "Demo_FogDensity", 0);
	SetEventHandler("Control Activation", "Demo_Control", 0);
	SetEventHandler("Location_CharacterIsFight", "Fight_IsEnabled", 0);
	SetEventHandler("ChrAttackAction", "Fight_AttackAction", 0);
	SetEventHandler("ChrCheckEnergy", "Fight_CheckEnergy", 0);
	SetEventHandler("ChrFgtActApply", "Fight_ActionApply", 0);
	SetEventHandler("Location_CharacterAttack", "Fight_Attack", 0);
	SetEventHandler("Location_Character_Dead", "Fight_DeathPlayed", 0);
	SetEventHandler("NpcEvtHP", "Fight_RelHP", 0);
	SetEventHandler("NpcEvtEny", "Fight_RelEnergy", 0);
	SetEventHandler("NpcEvtEgy", "Fight_RelEnergy", 0);
	SetEventHandler("NPC_Event_GetActionEnergy", "Fight_ActionEnergy", 0);
	SetEventHandler("NPC_Event_GetAttackActive", "Ai_AttackActive", 0);
	SetEventHandler("NPC_Event_GetAttackWeightFast", "Ai_WeightFast", 0);
	SetEventHandler("NPC_Event_GetAttackWeightForce", "Ai_WeightForce", 0);
	SetEventHandler("NPC_Event_GetAttackWeightRound", "Ai_Zero", 0);
	SetEventHandler("NPC_Event_GetAttackWeightBreak", "Ai_Zero", 0);
	SetEventHandler("NPC_Event_GetAttackWeightFeint", "Ai_Zero", 0);
	SetEventHandler("NPC_Event_GetDefenceActive", "Ai_DefenceActive", 0);
	SetEventHandler("NPC_Event_GetDefenceWeightBlock", "Ai_WeightBlock", 0);
	SetEventHandler("NPC_Event_GetDefenceWeightParry", "Ai_WeightParry", 0);
	SetEventHandler("NPC_Event_EnableRecoil", "Ai_Zero", 0);
	SetEventHandler("NPC_Event_GetFireActive", "Ai_Zero", 0);
	SetEventHandler("NPC_Event_EnableFire", "Ai_Zero", 0);
	SetEventHandler("NPC_Event_StunChance", "Ai_Zero", 0);
	SetEventHandler("NPC_Event_AdaptiveTargetSelect", "Ai_ZeroInt", 0);
	SetEventHandler("NPC_IsDodgeEnabled", "Ai_ZeroInt", 0);
	SetEventHandler("Demo_Tick", "Demo_Tick", 0);

	CreateEntity(&Camera, "locationcamera");
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);

	shadowDensity[0] = argb(255, 96, 96, 96);
	shadowDensity[1] = argb(255, 96, 96, 64);

	CreateLog();
	NewBout();
	PostEvent("Demo_Tick", TICK_MS);
	Trace("MELEE: scene loaded");
}

// ---------------------------------------------------------------- fencers

void SetupFencer(ref chr, string model)
{
	chr.hp = MAX_HP;
	chr.energy = MAX_ENERGY;
	// resource\models\characters\<model>.gm, both with resource\animation\fencer.ani
	if (!SendMessage(chr, "lss", MSG_CHARACTER_SETMODEL, model, "fencer"))
	{
		Trace("MELEE: " + model + " not loaded");
	}
	// the fencer has one action of a kind where the engine expects lists of up to four
	// (attack_fast.a1..a3...): every list names it once. Changing "actions" rereads them.
	chr.actions.idle.i1 = "idle_1";
	chr.actions.dead.d1 = "death_1";
	chr.actions.fightidle.i1 = "fight stand";
	chr.actions.fightwalk = "fight walk";
	chr.actions.fightbackwalk = "fight back walk";
	chr.actions.fightrun = "fight run";
	chr.actions.fightbackrun = "fight back run";
	chr.actions.attack_fast.a1 = "attack_fast_1";
	chr.actions.attack_force.a1 = "attack_force_1";
	chr.actions.attack_round.a1 = "attack_force_1";
	chr.actions.attack_break.a1 = "attack_force_1";
	chr.actions.attack_feint.a1 = "attack_fast_1";
	chr.actions.attack_feintc.a1 = "attack_fast_1";
	chr.actions.hit_attack.h1 = "hit_attack_1";
	chr.actions.parry.p1 = "parry_1";
	chr.actions.hit_feint = "hit_attack_1";
	chr.actions.hit_parry = "hit_parry";
	chr.actions.hit_round = "hit_attack_1";
	chr.actions.hit_shot = "hit_attack_1";
	chr.actions.HitNoFight = "hit_attack_1";
	chr.actions.block = "block";
	chr.actions.block_axe = "block";
	chr.actions.blockhit = "block_hit";
	chr.actions.blockaxehit = "block_hit";
	chr.actions.blockbreak = "hit_attack_1";
	chr.actions.recoil = "block_hit";
	chr.actions.strafeleft = "fight walk";
	chr.actions.straferight = "fight walk";
	chr.actions.fightdead.d1 = "death_1";
	chr.actions = "";
	// models\Ammo\saber.gm; the trail between its "start" and "end" labels fades in 0.25 s
	SendMessage(chr, "lsfll", MSG_CHARACTER_SETBLADE, "saber", 0.25, argb(64, 255, 255, 255), argb(0, 255, 255, 255));
}

void NewBout()
{
	bout++;
	boutOver = false;
	// a character that died stays dead: every bout creates both fencers anew
	if (bout > 1)
	{
		DeleteClass(&Sailor);
		DeleteClass(&Pirate);
	}
	Sailor.id = "sailor";
	CreateEntity(&Sailor, "player");
	SetupFencer(&Sailor, "sailor");
	Pirate.id = "pirate";
	CreateEntity(&Pirate, "npcharacter");
	SetupFencer(&Pirate, "pirate");
	SendMessage(&Sailor, "lffff", MSG_CHARACTER_TELEPORT_AY, 0.0, 0.0, -5.0, 0.0);
	SendMessage(&Pirate, "lffff", MSG_CHARACTER_TELEPORT_AY, 0.0, 0.0, 1.0, 3.14159);

	// the engine attacks only enemies: the groups hold who is whose
	SendMessage(&Groups, "sis", "MoveChr", &Sailor, "Player");
	SendMessage(&Groups, "sis", "MoveChr", &Pirate, "Pirates");
	SendMessage(&Groups, "ssss", "SetRelation", "Player", "Pirates", "enemy");

	SendMessage(&Camera, "li", MSG_CAMERA_SETTARGET, &Sailor);
	SendMessage(&Camera, "l", MSG_CAMERA_FOLLOW);

	// the pirate's skill: how fast he answers an attack with a block or a parry
	SendMessage(&Pirate, "lf", MSG_CHARACTER_SETFTGLEVEL, 0.5);
	SendMessage(&Pirate, "lsi", MSG_NPCHARACTER_SETTASK, "Fight", &Sailor);
	Log("Bout " + bout + ": E draws the saber, F fast, G force, Space block, Q parry");
}

// ---------------------------------------------------------------- the rules the engine asks

int Fight_IsEnabled()
{
	aref chr = GetEventData();
	return !boutOver;
}

float ActionCost(string action)
{
	if (action == "fast") { return 10.0; }
	if (action == "force") { return 22.0; }
	if (action == "parry") { return 14.0; }
	if (action == "hit_parry") { return 8.0; }
	if (action == "recoil") { return 12.0; }
	return 6.0;
}

// an attack starts only with the energy for it
int Fight_AttackAction()
{
	aref chr = GetEventData();
	string action = GetEventData();
	if (stf(chr.energy) < ActionCost(action)) { return 0; }
	return 1;
}

int Fight_CheckEnergy()
{
	aref chr = GetEventData();
	string action = GetEventData();
	if (stf(chr.energy) < ActionCost(action)) { return 0; }
	return 1;
}

// the engine applies a done action once: the energy goes
void Fight_ActionApply()
{
	aref chr = GetEventData();
	string action = GetEventData();
	float energy = stf(chr.energy) - ActionCost(action);
	if (energy < 0.0) { energy = 0.0; }
	chr.energy = energy;
}

// a saber reached an enemy: a block takes most of the blow, a force attack breaks through more
void Fight_Attack()
{
	aref attacker = GetEventData();
	aref enemy = GetEventData();
	string action = GetEventData();
	int isBlocked = GetEventData();
	if (boutOver || stf(enemy.hp) <= 0.0) { return; }
	float damage = 12.0;
	if (action == "force") { damage = 22.0; }
	if (isBlocked)
	{
		damage = damage * 0.15;
		if (action == "force") { damage = damage * 2.0; }
	}
	float hp = stf(enemy.hp) - damage;
	if (hp < 0.0) { hp = 0.0; }
	enemy.hp = hp;
	string how = "hits";
	if (isBlocked) { how = "hits the block of"; }
	Log(attacker.id + " " + how + " " + enemy.id + " (" + action + "), " + enemy.id + " " + makeint(hp) + " hp");
	if (hp <= 0.0)
	{
		boutOver = true;
		// "Dead" stops the character and plays his fightdead action
		SendMessage(enemy, "ls", MSG_NPCHARACTER_SETTASK, "Dead");
		if (enemy.id == "pirate") { Log("The pirate falls. R for a new bout"); }
		else { Log("The sailor falls. R for a new bout"); }
		Trace("MELEE: " + enemy.id + " killed by " + attacker.id);
	}
}

void Fight_DeathPlayed()
{
	aref chr = GetEventData();
	Trace("MELEE: " + chr.id + " death played");
}

// life and energy for the bars and the AI, 0..1
float Fight_RelHP()
{
	aref chr = GetEventData();
	return stf(chr.hp) / MAX_HP;
}

float Fight_RelEnergy()
{
	aref chr = GetEventData();
	return stf(chr.energy) / MAX_ENERGY;
}

float Fight_ActionEnergy()
{
	aref chr = GetEventData();
	string action = GetEventData();
	return ActionCost(action) / MAX_ENERGY;
}

// ---------------------------------------------------------------- the pirate's choices

float Ai_AttackActive() { aref chr = GetEventData(); return 0.6; }
float Ai_WeightFast() { aref chr = GetEventData(); return 0.7; }
float Ai_WeightForce() { aref chr = GetEventData(); return 0.3; }
float Ai_DefenceActive() { aref chr = GetEventData(); return 0.5; }
float Ai_WeightBlock() { aref chr = GetEventData(); return 0.7; }
float Ai_WeightParry() { aref chr = GetEventData(); return 0.3; }
float Ai_Zero() { aref chr = GetEventData(); return 0.0; }
int Ai_ZeroInt() { return 0; }

// ---------------------------------------------------------------- the demo around

void Demo_Tick()
{
	Regen(&Sailor);
	Regen(&Pirate);
	PostEvent("Demo_Tick", TICK_MS);
}

void Regen(ref chr)
{
	float energy = stf(chr.energy) + ENERGY_REGEN * TICK_MS / 1000.0;
	if (energy > MAX_ENERGY) { energy = MAX_ENERGY; }
	chr.energy = energy;
}

ref Demo_ShadowDensity()
{
	return &shadowDensity;
}

float Demo_FogDensity()
{
	return stf(Weather.Fog.Density);
}

void CreateLog()
{
	ScreenLog.Log.left = 16;
	ScreenLog.Log.up = 12;
	ScreenLog.Log.width = 760;
	ScreenLog.Log.height = 140;
	ScreenLog.Log.font = "DEMO_LOG";
	ScreenLog.Log.fontscale = 1.0;
	ScreenLog.Log.color = argb(0, 255, 245, 215);
	ScreenLog.Log.offsetString = 26;
	ScreenLog.Log.speed = 0.1;
	ScreenLog.Log.color_speed = 0.03;
	CreateEntity(&ScreenLog, "ILogAndActions");
	LayerAddObject(EXECUTE, &ScreenLog, 100);
	LayerAddObject(REALIZE, &ScreenLog, 65535);
	SendMessage(&ScreenLog, "lll", LOG_AND_ACTIONS_INIT, 0, 1);
}

void Log(string text)
{
	SendMessage(&ScreenLog, "lls", LOG_ADD_STRING, 0, text);
}

void Demo_Control()
{
	string control = GetEventData();
	if (control == "Demo_Restart")
	{
		NewBout();
	}
	if (control == "Demo_Quit")
	{
		Trace("MELEE: quit");
		ExitProgram();
	}
}
