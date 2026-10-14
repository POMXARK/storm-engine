// Enemy ship demo: the brig of the ship demo against a second brig steered by SEA_AI. Both ships are
// given to SEA_AI (AI_MESSAGE_ADD_SHIP) in two groups; the enemy gets the task to attack the player:
// it sails up to its attack distance, turns its broadside to the player and fires every side whose
// guns bear. The guns are the labels of the ship model, grouped by side as the sides (Cannons.Borts)
// of the ship type. SEA_AI asks the script for the reload time, the charge and the aim height of a
// gun and calls it back for every shot; the script launches the ball through AIBalls. Space fires
// the player's guns the same way: SEA_AI picks the sides that bear on the enemy and aims them.
// Both hulls are counted by the script; at zero the ship is reported dead to SEA_AI and sinks.
// R starts a new battle. A/D turn, W/S set and furl the sails, the mouse turns the camera, Escape
// quits.

#define EXECUTE 0
#define REALIZE 1
// shared/layers.h: mirrored into the sea reflection (sky in the first, ships in the second)
#define SEA_REFLECTION 15
#define SEA_REFLECTION2 16
// shared/layers.h: what the cannon balls and cameras trace against
#define SAILS_TRACE 19

// shared/sea_ai/script_defines.h
#define AI_MESSAGE_ADD_SHIP 51001
#define AI_MESSAGE_CANNON_FIRE 51002
#define AI_MESSAGE_SET_CAMERAS_ATTRIBUTE 51004
#define AI_MESSAGE_GROUP_SET_XYZ_AY 51010
#define AI_MESSAGE_SHIP_SET_TASK 51011
#define AI_MESSAGE_SET_LAYERS 51015
#define AI_MESSAGE_GROUP_SET_COMMANDER 51022
#define AI_MESSAGE_CHARACTER_DEAD 51026
#define AI_CAMERAS_SET_CAMERA 55001
#define AITASK_ATTACK 1
#define PRIMARY_TASK 0
#define RELATION_FRIEND 1
#define RELATION_ENEMY 3
#define SHIP_MAST_TOUCH_BALL 2

// "lsffffffl": name, position, angles, lifetime in ms (0 burns until the system ends itself)
#define PS_CREATE 10000

#define MSG_SOUND_PLAY 77003
#define MSG_SOUND_SET_MASTER_VOLUME 77021
#define PCM_3D 1
#define MP3_STEREO 2
#define VOLUME_FX 1
#define VOLUME_MUSIC 2

#define LOG_ADD_STRING 45020
#define LOG_AND_ACTIONS_INIT 45022

#define PI 3.14159265
// the muzzle speed of a ball in m/s and the highest elevation of the guns: together they give a
// reach of about 250 m (AICannon::CalcMaxFireDistance): after its broadside the enemy may fall
// back well past ATTACK_DISTANCE, a shorter reach leaves the player nothing to fire at
#define BALL_SPEED 80.0
#define GUN_ELEVATION 0.2
#define ATTACK_DISTANCE 110.0
// the guns of a side fire within this angle around its beam
#define FIRE_ZONE 1.4
#define RELOAD_TIME 8.0
// the guns of a broadside go off one after another within this time
#define FIRE_SPREAD 0.6
// the aim point is scattered around the target in a circle growing with the distance
#define SCATTER 0.04
#define HULL 100
#define HULL_HIT_DAMAGE 4
#define MAST_HIT_DAMAGE 0.34
#define ENEMY_SPEED 9.0
#define ENEMY_START_X 60.0
#define ENEMY_START_Z 260.0

int iShipPriorityExecute = 2;
int iShipPriorityRealize = 31;

object Render;
object Scene;
object Weather;
object Sky;
object Sea;
object Sail;
object SeaCameras;
object ShipCamera;
object ScreenLog;
object Particles;
object Sound;
// steers the enemy, aims and loads the guns of both ships
object AISea;
// what SEA_AI reads about the camera: OutsideCamera makes it aim the player's guns at the enemy
// rather than along the view
object AICameras;
object AIBalls;
object BaseShip;
// characters 0 and 1: the ship entities and SEA_AI find each other's owner by the index attribute
object Player;
object Enemy;
object SailTextures;

int battleOver = 0;
// the broadsides of the player; Ship_Bort_Fire comes inside the fire message, so a key press that
// does not change it found no side bearing on the enemy
int playerBroadsides = 0;
int shots = 0;
int hits = 0;

float fogHeight = 150.0;

void MapKey(string controlName, int keyCode)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
}

void Main()
{
	MapKey("Ship_TurnLeft", 65);
	MapKey("Ship_TurnRight", 68);
	MapKey("Ship_SailUp", 87);
	MapKey("Ship_SailDown", 83);
	MapKey("ShipCamera_Turn_H", 256);
	MapKey("ShipCamera_Turn_V", 257);
	MapKey("Demo_Fire", 32);
	MapKey("Demo_NewBattle", 82);
	MapKey("Demo_Quit", 27);

	Weather.Wind.Angle = 0.0;
	Weather.Wind.Speed = 10.0;

	Render.BackColor = argb(255, 0, 0, 0);
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	CreateWorld();
	CreateSail();
	CreateLog();
	CreateEffects();
	CreateBattle();
	CreateCamera();

	SetEventHandler("Control Activation", "Demo_Control", 0);
	Log("The enemy closes in. Space fires the guns that bear, R a new battle");
	Trace("AISHIP: scene loaded");
}

// Sounds, particles and the cannonballs. AIBalls reads the ball types once, on isDone.
void CreateEffects()
{
	CreateEntity(&Particles, "particles");

	CreateEntity(&Sound, "Sound");
	SendMessage(&Sound, "lfff", MSG_SOUND_SET_MASTER_VOLUME, 1.0, 0.5, 1.0);
	SendMessage(&Sound, "lslllllllf", MSG_SOUND_PLAY, "music\battle.wav", MP3_STEREO, VOLUME_MUSIC,
		false, true, false, 3000, 0, 1.0);

	// AIBalls takes the renderer from the helper of SEA_AI: SEA_AI comes first
	CreateSeaAI();
	CreateEntity(&AIBalls, "AIBalls");
	LayerAddObject(EXECUTE, &AIBalls, 10);
	LayerAddObject(REALIZE, &AIBalls, 10);
	AIBalls.Texture = "demo\ball.tga";
	AIBalls.SubTexX = 1;
	AIBalls.SubTexY = 1;
	AIBalls.SpeedMultiply = 1.0;
	AIBalls.BallFlySoundDistance = 0.0;
	AIBalls.BallFlySoundStereoMultiplyer = 1.0;
	AIBalls.Balls.Balls.SubTexIndex = 0;
	AIBalls.Balls.Balls.GoodIndex = 0;
	AIBalls.Balls.Balls.Size = 0.5;
	AIBalls.Balls.Balls.Weight = 1.0;
	AIBalls.isDone = "";

	SetEventHandler("Shp_HullHit", "Demo_HullHit", 0);
	SetEventHandler("Ball_WaterHit", "Demo_WaterHit", 0);
	SetEventHandler("ShpMastDamage", "Demo_MastDamage", 0);

	// SEA_AI asks these and reads the answer: without a handler it reads nothing and asserts
	SetEventHandler("GetRelationEvent", "Demo_Relation", 0);
	SetEventHandler("Shp_GetCurBallsNum", "Demo_BallsNum", 0);
	SetEventHandler("CannGRTime", "Demo_ReloadTime", 0);
	SetEventHandler("CannGFTime", "Demo_FireDelay", 0);
	SetEventHandler("CannGFHeight", "Demo_FireHeight", 0);
	SetEventHandler("CannLoad", "Demo_CannonLoad", 0);
	SetEventHandler("ShpGetBortFireDelta", "Demo_FireScatter", 0);
	// and calls these back
	SetEventHandler("CannFire", "Demo_CannonFire", 0);
	SetEventHandler("Ship_Bort_Fire", "Demo_BroadsideFired", 0);
	SetEventHandler("Shp_BortReloaded", "Demo_BroadsideReloaded", 0);
}

void CreateSeaAI()
{
	CreateEntity(&AISea, "SEA_AI");
	LayerAddObject(EXECUTE, &AISea, 0);
	LayerAddObject(REALIZE, &AISea, 0);
}

// SEA_AI and its two groups: the player alone, and the enemy facing him ENEMY_START_Z ahead. A
// ship is placed at the position of its group; isDone, after the last ship, settles the start
// positions. The task is given last, when SEA_AI knows both ships.
void CreateBattle()
{
	battleOver = 0;
	AICameras.Crosshair.OutsideCamera = 1;
	AICameras.Crosshair.Texture = "demo\ball.tga";
	SendMessage(&AISea, "la", AI_MESSAGE_SET_CAMERAS_ATTRIBUTE, &AICameras);

	PrepareCharacter(&Player, "Player", 0, "PlayerGroup");
	Player.MainCharacter = 1;
	PrepareCharacter(&Enemy, "Enemy", 1, "EnemyGroup");
	Enemy.Ship.Mode = "war";
	Enemy.Ship.SeaAI.Init.AttackDistance = ATTACK_DISTANCE;

	AddShip(&Player, "PlayerGroup", 0.0, 0.0, 0.0);
	AddShip(&Enemy, "EnemyGroup", ENEMY_START_X, ENEMY_START_Z, PI);
	AISea.isDone = "";

	SendMessage(&AISea, "lllaa", AI_MESSAGE_SHIP_SET_TASK, AITASK_ATTACK, PRIMARY_TASK, &Enemy, &Player);
}

// The character of a ship: its index, its group, the guns and the hull. Cannons.Borts gets a class
// per side from SEA_AI with the charge and the reach of the side.
void PrepareCharacter(ref character, string id, int index, string group)
{
	DeleteAttribute(character, "");
	character.id = id;
	character.index = index;
	character.nation = index;
	character.SeaAI.Group.Name = group;
	character.Ship.HP = HULL;
	character.Ship.Cannons.Type = 0;
	character.Ship.Cannons.SpeedV0 = BALL_SPEED;
	character.Ship.Cannons.Borts = "";
	character.Ship.Speed.z = 1.0;
}

void AddShip(ref character, string group, float x, float z, float heading)
{
	SendMessage(&AISea, "lsa", AI_MESSAGE_GROUP_SET_COMMANDER, group, character);
	SendMessage(&AISea, "lsffff", AI_MESSAGE_GROUP_SET_XYZ_AY, group, x, 0.0, z, heading);
	CreateEntity(character, "ship");
	SendMessage(&AISea, "liaa", AI_MESSAGE_ADD_SHIP, character, character, &BaseShip);
	SendMessage(character, "ls", 20510, "DLightModel");
}

// A new battle: deleting SEA_AI deletes its ships and their entities, then both are made again.
void Demo_NewBattle()
{
	DeleteClass(&AISea);
	CreateSeaAI();
	shots = 0;
	hits = 0;
	CreateBattle();
	SendMessage(&SeaCameras, "lia", AI_CAMERAS_SET_CAMERA, &ShipCamera, &Player);
	Log("A new battle");
	Trace("AISHIP: new battle");
}

int Demo_Relation()
{
	int index1 = GetEventData();
	int index2 = GetEventData();
	if (index1 == index2) { return RELATION_FRIEND; }
	return RELATION_ENEMY;
}

// the balls in the hold; a side fires only when there is a ball for each of its guns
int Demo_BallsNum()
{
	aref character = GetEventData();
	return 100;
}

float Demo_ReloadTime()
{
	aref character = GetEventData();
	return RELOAD_TIME + frnd() * 1.0;
}

// the delay between the order and the shot, different for every gun: the broadside rolls along
float Demo_FireDelay()
{
	aref character = GetEventData();
	return frnd() * FIRE_SPREAD;
}

// the guns aim this high above the waterline of the target: at the hull
float Demo_FireHeight()
{
	aref character = GetEventData();
	aref enemy = GetEventData();
	return 1.5;
}

// 1: a ball is rammed home
int Demo_CannonLoad()
{
	aref character = GetEventData();
	return 1;
}

float Demo_FireScatter()
{
	aref character = GetEventData();
	float x = GetEventData();
	float y = GetEventData();
	float z = GetEventData();
	float dx = x - stf(character.ship.pos.x);
	float dz = z - stf(character.ship.pos.z);
	return SCATTER * sqrt(dx * dx + dz * dz);
}

// "lisffffff": a side has been ordered to fire, at the point given
void Demo_BroadsideFired()
{
	int index = GetEventData();
	int ship = GetEventData();
	string bort = GetEventData();
	string side = "starboard";
	if (bort == "cannonl") { side = "port"; }
	if (index == sti(Player.index))
	{
		playerBroadsides++;
		Log("Our " + side + " broadside!");
	}
	else { Log("The enemy fires his " + side + " broadside!"); }
	Trace("AISHIP: " + index + " fires " + bort);
}

// "as": the guns of a side are loaded again
void Demo_BroadsideReloaded()
{
	aref character = GetEventData();
	string bort = GetEventData();
	if (sti(character.index) != sti(Player.index) || battleOver) { return; }
	if (bort == "cannonl") { Log("Port guns ready"); }
	else { Log("Starboard guns ready"); }
}

// "afffffffff": one gun goes off. SEA_AI gives the muzzle, the heading and elevation it has aimed
// the shot at and the reach of the gun; the ball is handed to AIBalls as in the battle demo.
void Demo_CannonFire()
{
	aref character = GetEventData();
	float x = GetEventData();
	float y = GetEventData();
	float z = GetEventData();
	float speed = GetEventData();
	float heading = GetEventData();
	float elevation = GetEventData();
	float gunHeading = GetEventData();
	float reach = GetEventData();
	float rawAngle = GetEventData();

	AIBalls.Type = "Balls";
	AIBalls.CharacterIndex = character.index;
	AIBalls.x = x;
	AIBalls.y = y;
	AIBalls.z = z;
	AIBalls.SpdV0 = speed;
	AIBalls.HeightMultiply = 1.0;
	AIBalls.SizeMultiply = 1.0;
	AIBalls.TimeSpeedMultiply = 1.0;
	AIBalls.CannonType = 0;
	AIBalls.MaxFireDistance = reach;
	AIBalls.Ang = elevation;
	AIBalls.Dir = heading;
	AIBalls.RawAng = rawAngle;
	AIBalls.Event = "";
	AIBalls.add = "";

	SendMessage(&Sound, "lsllllllfff", MSG_SOUND_PLAY, "cannon", PCM_3D, VOLUME_FX, false, false, false, 0, x, y, z);
	if (sti(character.index) == sti(Player.index)) { shots++; }
}

// "illffflf": the ship hit, the owner of the ball, the index of the ship hit and the point.
void Demo_HullHit()
{
	aref ship = GetEventData();
	int owner = GetEventData();
	int index = GetEventData();
	float x = GetEventData();
	float y = GetEventData();
	float z = GetEventData();
	SendMessage(&Particles, "lsffffffl", PS_CREATE, "explosion", x, y, z, 0.0, 0.0, 0.0, 0);
	string hit = "demo\hit1.wav";
	if (rand(1) == 1) { hit = "demo\hit2.wav"; }
	SendMessage(&Sound, "lsllllllfffff", MSG_SOUND_PLAY, hit, PCM_3D, VOLUME_FX, false, false, false, 0,
		x, y, z, 20.0, 500.0);
	if (battleOver || owner == index) { return; }
	if (index == sti(Enemy.index))
	{
		hits++;
		DamageHull(&Enemy, "The enemy");
	}
	else
	{
		DamageHull(&Player, "We are");
	}
}

void DamageHull(ref character, string who)
{
	int hull = sti(character.Ship.HP) - HULL_HIT_DAMAGE;
	if (hull < 0) { hull = 0; }
	character.Ship.HP = hull;
	Trace("AISHIP: " + character.id + " hull " + hull);
	if (hull > 0)
	{
		if (sti(character.index) == sti(Enemy.index)) { Log("Hit! Enemy hull " + hull + "%"); }
		else { Log("We are hit! Hull " + hull + "%"); }
		return;
	}
	battleOver = 1;
	// SEA_AI stops steering and firing the ship and sinks it
	SendMessage(&AISea, "la", AI_MESSAGE_CHARACTER_DEAD, character);
	if (sti(character.index) == sti(Enemy.index))
	{
		Log("The enemy sinks! " + hits + " hits of " + shots + " shots, R for a new battle");
		Trace("AISHIP: enemy sinks after " + hits + " hits of " + shots + " shots");
		return;
	}
	Log("We are sinking! R for a new battle");
	Trace("AISHIP: player sinks");
}

void Demo_WaterHit()
{
	int owner = GetEventData();
	float x = GetEventData();
	float y = GetEventData();
	float z = GetEventData();
	SendMessage(&Particles, "lsffffffl", PS_CREATE, "splash", x, y, z, 0.0, 0.0, 0.0, 0);
	SendMessage(&Sound, "lsllllllfffff", MSG_SOUND_PLAY, "demo\splash.wav", PCM_3D, VOLUME_FX, false, false, false, 0,
		x, y, z, 15.0, 300.0);
}

// The ship asks the script how much a mast is damaged after a touch; it breaks at 1.0.
float Demo_MastDamage()
{
	int touch = GetEventData();
	int mast = GetEventData();
	float x = GetEventData();
	float y = GetEventData();
	float z = GetEventData();
	float damage = GetEventData();
	if (touch != SHIP_MAST_TOUCH_BALL) { return damage; }
	return damage + MAST_HIT_DAMAGE;
}

void CreateWorld()
{
	CreateEntity(&Weather, "weather");
	Weather.Fog.Enable = 1;
	Weather.Fog.Start = 0.0;
	Weather.Fog.Density = 0.002;
	Weather.Fog.Color = argb(0, 150, 185, 215);
	Weather.Sun.Color = argb(0, 230, 220, 205);
	Weather.Sun.Ambient = argb(0, 95, 100, 105);
	Weather.Sun.HeightAngle = 0.7;
	Weather.Sun.AzimuthAngle = 2.618;
	// the entity takes the wind only when it is assigned after its creation (AttributeChanged)
	Weather.Wind.Angle = stf(Weather.Wind.Angle);
	Weather.Wind.Speed = stf(Weather.Wind.Speed);
	Weather.isDone = "";

	SetEventHandler("WhrCalcFogColor", "Demo_FogColor", 0);
	CreateEntity(&Sky, "Sky");
	Sky.Size = 2048.0;
	Sky.Angle = 0.0;
	Sky.RotateSpeed = 0.0;
	Sky.techSky = "Sky";
	Sky.techSkyBlend = "SkyBlend";
	Sky.techSkyAlpha = "skyblend_alpha";
	Sky.techSkyFog = "SkyFog";
	LayerAddObject(EXECUTE, &Sky, 2);
	LayerAddObject(REALIZE, &Sky, 3);
	LayerAddObject(SEA_REFLECTION, &Sky, 1);
	Sky.Dir.d0 = "weather\skies\day\";
	Sky.Dir = "12";
	Sky.Color = argb(0, 255, 255, 255);
	Sky.isDone = "";

	CreateEntity(&Sea, "sea");
	LayerAddObject(EXECUTE, &Sea, 4);
	LayerAddObject(REALIZE, &Sea, 65530);
	Sea.Sea2.WaterColor = argb(0, 30, 90, 115);
	Sea.Sea2.SkyColor = argb(0, 205, 255, 255);
	Sea.Sea2.Amp1 = 1.8;
	Sea.Sea2.AnimSpeed1 = 1.5;
	Sea.Sea2.Scale1 = 0.6;
	Sea.Sea2.MoveSpeed1 = "2.0, 0.0, 0.0";
	Sea.Sea2.Amp2 = 0.6;
	Sea.Sea2.AnimSpeed2 = 15.0;
	Sea.Sea2.Scale2 = 3.8;
	Sea.Sea2.MoveSpeed2 = "1.0, 0.0, 1.0";
	Sea.Sea2.PosShift = 1.0;
	Sea.Sea2.BumpScale = 0.05;
	Sea.Sea2.Reflection = 0.6;
	Sea.Sea2.Transparency = 0.45;
	Sea.Sea2.Frenel = 0.45;
	Sea.Sea2.Attenuation = 0.3;
	Sea.Sea2.LodScale = 0.5;
	Sea.Sea2.GridStep = 0.1;
	Sea.Sea2.FoamEnable = 1;
	Sea.Sea2.FoamK = 0.2;
	Sea.Sea2.FoamV = 2.0;
	Sea.Sea2.FoamUV = 0.5;
	Sea.Sea2.FoamTexDisturb = 0.7;
	Sea.Sea2.SimpleSea = 0;
	Sea.Fog.Enable = 1;
	Sea.Fog.Start = 0.0;
	Sea.Fog.Color = argb(0, 150, 185, 215);
	Sea.Fog.SeaDensity = 0.001;
	Sea.MaxSeaHeight = 50.0;
	Sea.isDone = "";
}

// The sail entity serves the sails of every ship; the ship type is shared by both brigs. Its sides
// name the label groups of the gun ports in the model: FireDir is the beam of the side (from the
// bow, clockwise), FireZone the arc the guns cover, FireAngMax their highest elevation.
void CreateSail()
{
	CreateEntity(&Sail, "sail");
	LayerAddObject(EXECUTE, &Sail, iShipPriorityExecute + 1);
	LayerAddObject(REALIZE, &Sail, iShipPriorityRealize + 1);
	LayerAddObject(SAILS_TRACE, &Sail, 10);
	LayerAddObject(SEA_REFLECTION2, &Sail, 3);
	SailTextures.normalTex = "ships\brig\sail.tga";

	BaseShip.Name = "Brig";
	BaseShip.Class = 4;
	BaseShip.HP = HULL;
	BaseShip.SpeedRate = 12.0;
	BaseShip.TurnRate = 40.0;
	BaseShip.MinCrew = 20;
	BaseShip.MaxCrew = 120;
	BaseShip.Capacity = 1500;
	BaseShip.Weight = 300.0;
	BaseShip.SpeedDependWeight = 0.2;
	BaseShip.SubSeaDependWeight = 0.6;
	BaseShip.TurnDependWeight = 0.2;
	BaseShip.TurnDependSpeed = 0.3;
	BaseShip.WaterLine = 0.0;
	BaseShip.InertiaAccelerationX = 0.2;
	BaseShip.InertiaBrakingX = 2.0;
	BaseShip.InertiaAccelerationY = 6.0;
	BaseShip.InertiaBrakingY = 3.0;
	BaseShip.InertiaAccelerationZ = 4.0;
	BaseShip.InertiaBrakingZ = 3.0;
	BaseShip.Cannons.Borts.cannonl.FireZone = FIRE_ZONE;
	BaseShip.Cannons.Borts.cannonl.FireDir = PI * 1.5;
	BaseShip.Cannons.Borts.cannonl.FireAngMin = -0.1;
	BaseShip.Cannons.Borts.cannonl.FireAngMax = GUN_ELEVATION;
	BaseShip.Cannons.Borts.cannonr.FireZone = FIRE_ZONE;
	BaseShip.Cannons.Borts.cannonr.FireDir = PI * 0.5;
	BaseShip.Cannons.Borts.cannonr.FireAngMin = -0.1;
	BaseShip.Cannons.Borts.cannonr.FireAngMax = GUN_ELEVATION;

	SetEventHandler("SeaGetLayers", "Demo_ShipLayers", 0);
	SetEventHandler("Shp_UpdParams", "Demo_ShipParameters", 0);
	SetEventHandler("GetSailTextureData", "Demo_SailTextures", 0);
}

void CreateCamera()
{
	CreateEntity(&SeaCameras, "SEA_CAMERAS");
	CreateEntity(&ShipCamera, "SHIP_CAMERA");
	ShipCamera.Perspective = 1.285;
	ShipCamera.SensivityDistance = 30.0;
	ShipCamera.SensivityHeightAngle = 0.02;
	ShipCamera.SensivityHeightAngleOnShip = 0.005;
	ShipCamera.SensivityAzimuthAngle = 0.04;
	ShipCamera.MaxAngleX = -0.2;
	ShipCamera.MinAngleX = -1.4;
	ShipCamera.Distance = 0.5;
	ShipCamera.MinDistance = 25.0;
	ShipCamera.MaxDistance = 1500.0;
	ShipCamera.MinHeightOnSea = 1.0;
	ShipCamera.MaxHeightOnShip = 5.0;
	ShipCamera.InvertMouseX = 1.0;
	ShipCamera.InvertMouseY = -1.0;
	SendMessage(&SeaCameras, "lia", AI_CAMERAS_SET_CAMERA, &ShipCamera, &Player);
	SendMessage(&Scene, "si", "SetActiveCamera", &ShipCamera);
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

void Demo_ShipLayers()
{
	aref ship = GetEventData();
	SendMessage(ship, "lll", AI_MESSAGE_SET_LAYERS, EXECUTE, REALIZE);
}

// "lf": the sails of a ship have changed: SEA_AI sets them for the enemy, the keys for the player.
// The top speed is scaled by the sails in the ship; the turn rate is given here.
void Demo_ShipParameters()
{
	int index = GetEventData();
	float sails = GetEventData();
	float turn = stf(BaseShip.TurnRate) / 160.0 * (0.4 + 0.6 * sails);
	if (index == sti(Player.index))
	{
		Player.Ship.MaxSpeedZ = stf(BaseShip.SpeedRate);
		Player.Ship.MaxSpeedY = turn;
		return;
	}
	// the enemy sails a little slower than the player, who can run or close in
	Enemy.Ship.MaxSpeedZ = ENEMY_SPEED;
	Enemy.Ship.MaxSpeedY = turn;
}

ref Demo_SailTextures()
{
	int index = GetEventData();
	return &SailTextures;
}

int Demo_FogColor()
{
	float x = GetEventData();
	float y = GetEventData();
	float z = GetEventData();
	float k = y / fogHeight;
	if (k < 0.0) { k = 0.0; }
	if (k > 1.0) { k = 1.0; }
	int alpha = 255 - makeint(255.0 * k);
	return or(shl(alpha, 24), sti(Weather.Fog.Color));
}

void Demo_Control()
{
	string control = GetEventData();
	if (control == "Demo_Fire" && !battleOver)
	{
		int before = playerBroadsides;
		SendMessage(&AISea, "lal", AI_MESSAGE_CANNON_FIRE, &Player, 1);
		if (playerBroadsides == before) { Log("No loaded side bears on the enemy"); }
	}
	if (control == "Demo_NewBattle") { Demo_NewBattle(); }
	if (control == "Demo_Quit")
	{
		Trace("AISHIP: quit");
		ExitProgram();
	}
}
