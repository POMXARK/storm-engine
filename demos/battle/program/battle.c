// Battle demo: the brig of the ship demo against a second brig drifting under sail. Q and E fire a
// broadside to port and to starboard: the script places one cannonball per gun along the side and
// hands it to AIBalls, which flies it and traces it against the ships, their sails and the sea.
// The ship entity reports a hit on the hull (Shp_HullHit) and leaves a shot hole on it, a ball
// through a sail tears a hole in the cloth, a hit mast asks the script for its damage and breaks,
// a ball into the sea raises Ball_WaterHit. The script shows a blast or a splash with the particles
// of the particles demo, plays the sounds and counts the hull points of the target; at zero the
// target sinks. R brings a new target. A/D turn, W/S set and furl the sails, the mouse turns the
// camera, Escape quits.

#define EXECUTE 0
#define REALIZE 1
// shared/layers.h: mirrored into the sea reflection (sky in the first, ships in the second)
#define SEA_REFLECTION 15
#define SEA_REFLECTION2 16
// shared/layers.h: what the cannon balls and cameras trace against
#define SAILS_TRACE 19

// shared/messages.h and shared/sea_ai/script_defines.h
#define MSG_SHIP_CREATE 50300
// "l": the ship sinks; added for scripts that make ships without SEA_AI
#define MSG_SHIP_SET_DEAD 50316
#define AI_MESSAGE_SET_LAYERS 51015
#define AI_CAMERAS_SET_CAMERA 55001
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
// guns per side, spread along the hull from GUN_AFT to GUN_FORE, GUN_SIDE off the keel line
#define GUNS 6
#define GUN_AFT -8.0
#define GUN_FORE 7.0
#define GUN_SIDE 3.9
#define GUN_HEIGHT 2.6
// a ball leaves the muzzle at this speed in m/s; with the elevation it falls into the sea about
// 90 m away, the distance the target sails at
#define BALL_SPEED 80.0
#define GUN_ELEVATION 0.055
// seconds between the guns of a broadside and before a side can fire again
#define GUN_INTERVAL 0.12
#define RELOAD_TIME 4.0
#define TARGET_HULL 100
#define TARGET_RANGE 85.0
#define TARGET_AHEAD 10.0
#define TARGET_SPEED 6.0
#define HULL_HIT_DAMAGE 5
// a mast breaks after this many hits
#define MAST_HIT_DAMAGE 0.34

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
// SEA_AI is created only for its helper: AIBalls takes the renderer from it
object AISea;
// the cannonballs in flight, of every type listed in AIBalls.Balls
object AIBalls;
object BaseShip;
// characters 0 and 1: the ship entities find each other's owner by the index attribute
object Player;
object Target;
object SailTextures;

// set in Main: a global initialised from a #define stays 0
int targetHull = 0;
int targetSunk = 0;
// the broadside being fired: side (-1 port, 1 starboard), the next gun and the time to it
int broadsideSide = 0;
int broadsideGun = 0;
float broadsideTimer = 0.0;
float reloadPort = 0.0;
float reloadStarboard = 0.0;
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
	MapKey("Demo_FirePort", 81);
	MapKey("Demo_FireStarboard", 69);
	MapKey("Demo_NewTarget", 82);
	MapKey("Demo_Quit", 27);

	targetHull = TARGET_HULL;
	Weather.Wind.Angle = 0.0;
	Weather.Wind.Speed = 10.0;

	Render.BackColor = argb(255, 0, 0, 0);
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	CreateWorld();
	CreateSail();
	CreatePlayer();
	CreateTarget();
	CreateCamera();
	CreateLog();
	CreateBattle();

	SetEventHandler("frame", "Demo_Frame", 0);
	SetEventHandler("Control Activation", "Demo_Control", 0);
	Log("Q port broadside, E starboard, R new target");
	Trace("BATTLE: scene loaded");
}

// Cannonballs, particles and sounds. AIBalls reads the ball types once, on isDone: every type has
// a sprite in the texture atlas and the size it is drawn at.
void CreateBattle()
{
	CreateEntity(&AISea, "SEA_AI");

	CreateEntity(&Particles, "particles");

	CreateEntity(&Sound, "Sound");
	SendMessage(&Sound, "lfff", MSG_SOUND_SET_MASTER_VOLUME, 1.0, 0.5, 1.0);
	SendMessage(&Sound, "lslllllllf", MSG_SOUND_PLAY, "music\battle.wav", MP3_STEREO, VOLUME_MUSIC,
		false, true, false, 3000, 0, 1.0);

	CreateEntity(&AIBalls, "AIBalls");
	LayerAddObject(EXECUTE, &AIBalls, 10);
	LayerAddObject(REALIZE, &AIBalls, 10);
	AIBalls.Texture = "demo\ball.tga";
	AIBalls.SubTexX = 1;
	AIBalls.SubTexY = 1;
	AIBalls.SpeedMultiply = 1.0;
	// a ball passing the camera closer than this raises Bll_FlyNCam (a whistle in the game)
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
	SetEventHandler("EventMastFall", "Demo_MastFall", 0);
	SetEventHandler("ShpSailDamage", "Demo_SailDamage", 0);
	SetEventHandler("ShpDelete", "Demo_ShipDeleted", 0);
}

// One gun of a broadside. The muzzle is on the side of the hull, turned with the ship; the ball is
// given to AIBalls through its attributes and "add". Dir is the heading of the shot (the angle from
// +z, as ship.ang.y), Ang its elevation; a little scatter makes the broadside spread.
void FireGun(int side, int gun)
{
	float heading = stf(Player.ship.ang.y);
	float c = cos(heading);
	float s = sin(heading);
	float along = GUN_AFT + (GUN_FORE - GUN_AFT) * makefloat(gun) / makefloat(GUNS - 1);
	float across = GUN_SIDE * makefloat(side);
	// the ship's +x is starboard; local x and z turned by the heading into the world
	float x = stf(Player.ship.pos.x) + across * c + along * s;
	float z = stf(Player.ship.pos.z) - across * s + along * c;
	float y = stf(Player.ship.pos.y) + GUN_HEIGHT;

	AIBalls.Type = "Balls";
	AIBalls.CharacterIndex = Player.index;
	AIBalls.x = x;
	AIBalls.y = y;
	AIBalls.z = z;
	AIBalls.SpdV0 = BALL_SPEED;
	AIBalls.HeightMultiply = 1.0;
	AIBalls.SizeMultiply = 1.0;
	AIBalls.TimeSpeedMultiply = 1.0;
	AIBalls.CannonType = 0;
	AIBalls.MaxFireDistance = 1000.0;
	AIBalls.Ang = GUN_ELEVATION + (frnd() - 0.5) * 0.01;
	AIBalls.Dir = heading + makefloat(side) * PI * 0.5 + (frnd() - 0.5) * 0.04;
	AIBalls.RawAng = 0.0;
	AIBalls.Event = "";
	AIBalls.add = "";

	// the cannon alias of the sound demo picks one of three shots
	SendMessage(&Sound, "lsllllllfff", MSG_SOUND_PLAY, "cannon", PCM_3D, VOLUME_FX, false, false, false, 0, x, y, z);
	shots++;
}

void Demo_FireBroadside(int side)
{
	if (broadsideSide != 0) { return; }
	if (side < 0 && reloadPort > 0.0) { Log("Port guns reloading"); return; }
	if (side > 0 && reloadStarboard > 0.0) { Log("Starboard guns reloading"); return; }
	broadsideSide = side;
	broadsideGun = 0;
	broadsideTimer = 0.0;
	if (side < 0) { Log("Port broadside!"); reloadPort = RELOAD_TIME; }
	else { Log("Starboard broadside!"); reloadStarboard = RELOAD_TIME; }
	Trace("BATTLE: broadside " + side);
}

void Demo_Frame()
{
	float dt = GetDeltaTime() * 0.001;
	if (reloadPort > 0.0) { reloadPort = reloadPort - dt; }
	if (reloadStarboard > 0.0) { reloadStarboard = reloadStarboard - dt; }
	if (broadsideSide == 0) { return; }
	broadsideTimer = broadsideTimer - dt;
	while (broadsideSide != 0 && broadsideTimer <= 0.0)
	{
		FireGun(broadsideSide, broadsideGun);
		broadsideGun++;
		broadsideTimer = broadsideTimer + GUN_INTERVAL;
		if (broadsideGun >= GUNS) { broadsideSide = 0; }
	}
}

// "illffflf": the ship hit, the owner of the ball, the index of the ship hit, the point, and the
// fire place nearest to it with the distance (the game sets the hull on fire there).
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
	hits++;
	if (index != sti(Target.index) || targetSunk) { return; }
	targetHull = targetHull - HULL_HIT_DAMAGE;
	if (targetHull > 0)
	{
		Log("Hit! Hull " + targetHull + "%");
		Trace("BATTLE: hull hit, hull " + targetHull);
		return;
	}
	targetHull = 0;
	targetSunk = 1;
	SendMessage(&Target, "l", MSG_SHIP_SET_DEAD);
	Log("The target sinks! " + hits + " hits of " + shots + " shots, R for a new one");
	Trace("BATTLE: target sinks after " + hits + " hits of " + shots + " shots");
}

// "lfff": the owner of the ball and the point on the water.
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

// The ship asks the script how much a mast is damaged after a touch (a ball, another ship, an
// island). It breaks at 1.0, so every ball adds a third. Without a handler the engine would read
// the answer from nothing.
float Demo_MastDamage()
{
	int touch = GetEventData();
	int mast = GetEventData();
	float x = GetEventData();
	float y = GetEventData();
	float z = GetEventData();
	float damage = GetEventData();
	if (touch != SHIP_MAST_TOUCH_BALL) { return damage; }
	damage = damage + MAST_HIT_DAMAGE;
	if (damage < 1.0) { Log("Mast " + mast + " hit"); }
	Trace("BATTLE: mast " + mast + " damage " + damage);
	return damage;
}

// "lsl": the character, the node of the mast and whether it fell from damage below 1 (it did not).
void Demo_MastFall()
{
	int index = GetEventData();
	string node = GetEventData();
	SendMessage(&Sound, "lsllllllfffff", MSG_SOUND_PLAY, "demo\crack.wav", PCM_3D, VOLUME_FX, false, false, false, 0,
		stf(Target.ship.pos.x), 10.0, stf(Target.ship.pos.z), 20.0, 500.0);
	Log("The " + node + " falls!");
	Trace("BATTLE: mast falls " + node);
}

// "lfffl": the index of the ship, the point on the sail, the owner of the ball. The hole itself is
// made by the sail entity.
void Demo_SailDamage()
{
	int index = GetEventData();
	Trace("BATTLE: sail hit");
}

// "li": the sunk ship has gone deep and taken itself out of the layers.
void Demo_ShipDeleted()
{
	int index = GetEventData();
	Trace("BATTLE: ship " + index + " gone");
}

// A new target: the old ship entity, sunk or not, is deleted and made again with full hull.
void Demo_NewTarget()
{
	DeleteClass(&Target);
	targetHull = TARGET_HULL;
	targetSunk = 0;
	CreateTarget();
	Log("A new target");
	Trace("BATTLE: new target");
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

// The sail entity serves the sails of every ship; the ship type is shared by both brigs.
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

	SetEventHandler("SeaGetLayers", "Demo_ShipLayers", 0);
	SetEventHandler("Shp_UpdParams", "Demo_ShipParameters", 0);
	SetEventHandler("GetSailTextureData", "Demo_SailTextures", 0);
}

void CreatePlayer()
{
	Player.id = "Player";
	Player.index = 0;
	Player.nation = 0;
	Player.MainCharacter = 1;
	Player.ship.pos.x = 0.0;
	Player.ship.pos.z = 0.0;
	Player.ship.ang.y = 0.0;
	Player.Ship.Speed.z = 1.0;
	CreateEntity(&Player, "ship");
	SendMessage(&Player, "laa", MSG_SHIP_CREATE, &Player, &BaseShip);
	SendMessage(&Player, "ls", 20510, "DLightModel");
	Demo_UpdateSpeeds(0, 1.0);
}

// The target is not the main character, so it reads no controls: its sails stay set and it sails
// ahead at the top speed the script gives it. It comes up on the player's starboard beam,
// TARGET_RANGE off and a little ahead, on the player's heading.
void CreateTarget()
{
	float heading = stf(Player.ship.ang.y);
	float c = cos(heading);
	float s = sin(heading);
	Target.id = "Target";
	Target.index = 1;
	Target.nation = 1;
	Target.ship.pos.x = stf(Player.ship.pos.x) + TARGET_RANGE * c + TARGET_AHEAD * s;
	Target.ship.pos.z = stf(Player.ship.pos.z) - TARGET_RANGE * s + TARGET_AHEAD * c;
	Target.ship.ang.y = heading;
	Target.Ship.Speed.z = 1.0;
	CreateEntity(&Target, "ship");
	SendMessage(&Target, "laa", MSG_SHIP_CREATE, &Target, &BaseShip);
	SendMessage(&Target, "ls", 20510, "DLightModel");
	Demo_UpdateSpeeds(1, 1.0);
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
	// kept above the deck, so the target is seen over the player's ship
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

void Demo_ShipParameters()
{
	int index = GetEventData();
	float sails = GetEventData();
	Demo_UpdateSpeeds(index, sails);
}

void Demo_UpdateSpeeds(int index, float sails)
{
	if (index == 0)
	{
		Player.Ship.MaxSpeedZ = stf(BaseShip.SpeedRate);
		Player.Ship.MaxSpeedY = stf(BaseShip.TurnRate) / 160.0 * (0.4 + 0.6 * sails);
		return;
	}
	// the target sails slower than the player, who can keep abeam of it with half sails
	Target.Ship.MaxSpeedZ = TARGET_SPEED;
	Target.Ship.MaxSpeedY = 0.0;
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
	if (control == "Demo_FirePort") { Demo_FireBroadside(-1); }
	if (control == "Demo_FireStarboard") { Demo_FireBroadside(1); }
	if (control == "Demo_NewTarget") { Demo_NewTarget(); }
	if (control == "Demo_Quit")
	{
		Trace("BATTLE: quit");
		ExitProgram();
	}
}
