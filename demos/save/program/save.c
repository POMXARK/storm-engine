// Save demo: the brig of the ship demo at sea, saved and restored by the engine itself.
// SaveEngineState writes every script variable with all its attributes into one file; the ship
// entity keeps its position and heading in the attributes of its character, the weather keeps the
// wind in its own, so they go into the file with the rest. LoadEngineState removes all entities,
// compiles the program again, restores the variables and calls OnLoad, which builds the scene
// anew from them. F5 saves, F9 loads, A/D turn, W/S set and furl the sails, the mouse turns the
// camera, Left/Right turn the wind, Escape quits. The on-screen log reports what happened.

#define EXECUTE 0
#define REALIZE 1
// shared/layers.h: mirrored into the sea reflection (sky in the first, ships in the second)
#define SEA_REFLECTION 15
#define SEA_REFLECTION2 16
// shared/layers.h: what the cannon balls and cameras trace against
#define SAILS_TRACE 19

// shared/messages.h and shared/sea_ai/script_defines.h
#define MSG_SHIP_CREATE 50300
#define MSG_SHIP_SET_SAIL_STATE 50305
#define MSG_SHIP_GET_SAIL_STATE 50325
#define AI_MESSAGE_SET_LAYERS 51015
#define AI_CAMERAS_SET_CAMERA 55001

// shared/battle_interface/log_msg.h
// "lls": immortal (stays until replaced), text
#define LOG_ADD_STRING 45020
// "lll": show the active action icon, show the log lines
#define LOG_AND_ACTIONS_INIT 45022

// the file is looked up through the resource paths, so it must be under save\ (see run.sh)
#define SAVE_FILE "save\ship.sav"

// priorities of the ship in the execute and realize layers; the ship entity reads these globals
// by name and its sails, blots and camera are placed right after it
int iShipPriorityExecute = 2;
int iShipPriorityRealize = 31;

// the description written into the header of the save file (EXTDATA_HEADER::sFileInfo)
string savefile_info = "";

object Render;
object Scene;
object Weather;
object Sky;
object Sea;
object Sail;
object SeaCameras;
object ShipCamera;
object ScreenLog;
// lists the files of a folder into its attributes when created
object SaveFinder;
// the ship type: the engine reads its parameters and the model name, ships\<Name>\<Name>.gm
object BaseShip;
// the character the ship entity lives on
object Player;
// textures of the sails, returned to the sail entity by GetSailTextureData
object SailTextures;

// plain globals are saved too; the sails live inside the ship entity, so OnSave copies them here
int saveCount = 0;
float sailState = 1.0;

float fogHeight = 150.0;

void MapKey(string controlName, int keyCode)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
}

// Runs once at start. After a load the engine calls OnLoad instead.
void Main()
{
	// read by the ship entity itself for the main character
	MapKey("Ship_TurnLeft", 65);
	MapKey("Ship_TurnRight", 68);
	MapKey("Ship_SailUp", 87);
	MapKey("Ship_SailDown", 83);
	// read by SHIP_CAMERA; 256/257 are the mouse axes
	MapKey("ShipCamera_Turn_H", 256);
	MapKey("ShipCamera_Turn_V", 257);
	MapKey("ShipCamera_Forward", 81);
	MapKey("ShipCamera_Backward", 69);
	MapKey("Demo_WindLeft", 37);
	MapKey("Demo_WindRight", 39);
	MapKey("Demo_Save", 116);
	MapKey("Demo_Load", 120);
	MapKey("Demo_Quit", 27);

	// the state a new game starts with; a load brings back the saved one instead
	Weather.Wind.Angle = 0.0;
	Weather.Wind.Speed = 10.0;
	Player.ship.pos.x = 0.0;
	Player.ship.pos.z = 0.0;
	Player.ship.ang.y = 0.0;
	// a positive speed starts with the sails set
	Player.Ship.Speed.z = 1.0;

	CreateScene();
	Log("F5 save, F9 load, A/D helm, W/S sails, arrows wind");
	if (SaveExists()) { Log("A save is there, F9 loads it"); }
	Trace("SAVE: scene loaded");
}

// Called by the engine after LoadEngineState has restored the variables. Entities and event
// handlers are gone, the controls service keeps its controls.
void OnLoad()
{
	CreateScene();
	SendMessage(&Player, "lf", MSG_SHIP_SET_SAIL_STATE, sailState);
	Log("Loaded save #" + saveCount + ": " + Describe());
	Trace("SAVE: loaded #" + saveCount + " " + Describe());
}

// Called by SaveEngineState before the variables are written.
void OnSave()
{
	SendMessage(&Player, "le", MSG_SHIP_GET_SAIL_STATE, &sailState);
	savefile_info = "Brig, save #" + saveCount + ", " + Describe();
}

void CreateScene()
{
	Render.BackColor = argb(255, 0, 0, 0);
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);

	CreateWorld();
	CreateShip();
	CreateCamera();
	CreateLog();

	SetEventHandler("Control Activation", "Demo_Control", 0);
}

// Sea, sky and weather as in the ship demo. The wind is set by Main or restored by the load.
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
	// the entity takes a value only when the attribute is assigned while it exists (AttributeChanged);
	// the wind set by Main or restored by the load is an attribute it has never seen, so it is
	// assigned again, otherwise the entity keeps garbage and the sea and the sails break
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
	Sea.Sea2.Amp1 = 3.0;
	Sea.Sea2.AnimSpeed1 = 1.5;
	Sea.Sea2.Scale1 = 0.6;
	Sea.Sea2.MoveSpeed1 = "2.0, 0.0, 0.0";
	Sea.Sea2.Amp2 = 0.8;
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

// The ship as in the ship demo. It is placed where Player.ship.pos and Player.ship.ang say: the
// start position after Main, the saved one after a load.
void CreateShip()
{
	// sail reads resource\ini\rigging.ini; it is drawn and traced right after the ship
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

	Player.id = "Player";
	Player.index = 0;
	Player.nation = 0;
	// the ship of the main character reads the Ship_* controls itself
	Player.MainCharacter = 1;

	SetEventHandler("SeaGetLayers", "Demo_ShipLayers", 0);
	SetEventHandler("Shp_UpdParams", "Demo_ShipParameters", 0);
	SetEventHandler("GetSailTextureData", "Demo_SailTextures", 0);
	CreateEntity(&Player, "ship");
	SendMessage(&Player, "laa", MSG_SHIP_CREATE, &Player, &BaseShip);
	// lit by the sun of the weather; without a technique the vertex colours are drawn as is
	SendMessage(&Player, "ls", 20510, "DLightModel");
	Demo_UpdateSpeeds(sailState);
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
	ShipCamera.MaxAngleX = 0.07;
	ShipCamera.MinAngleX = -1.4;
	ShipCamera.Distance = 0.05;
	ShipCamera.MinDistance = 25.0;
	ShipCamera.MaxDistance = 1500.0;
	ShipCamera.MinHeightOnSea = 1.0;
	ShipCamera.MaxHeightOnShip = 5.0;
	ShipCamera.InvertMouseX = 1.0;
	ShipCamera.InvertMouseY = -1.0;
	SendMessage(&SeaCameras, "lia", AI_CAMERAS_SET_CAMERA, &ShipCamera, &Player);
	SendMessage(&Scene, "si", "SetActiveCamera", &ShipCamera);
}

// The log of the game's battle interface: lines appear at the top left, move up as new ones come
// and fade out. The font is a section of resource\ini\fonts.ini written by make_save.py.
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
	// pixels and alpha units per millisecond: a line moves up quickly and fades in about 8 s
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

// FINDFILESINTODIRECTORY fills filelist.id0, id1... with the files of dir matching mask when it
// is created; it does nothing after that, so it is removed at once.
bool SaveExists()
{
	DeleteAttribute(&SaveFinder, "");
	SaveFinder.dir = "save";
	SaveFinder.mask = "ship.sav";
	CreateEntity(&SaveFinder, "FINDFILESINTODIRECTORY");
	DeleteClass(&SaveFinder);
	aref files;
	makearef(files, SaveFinder.filelist);
	return GetAttributesNum(files) > 0;
}

void Demo_Save()
{
	saveCount = saveCount + 1;
	SaveEngineState(SAVE_FILE);
	Log("Saved #" + saveCount + ": " + Describe());
	Trace("SAVE: saved #" + saveCount + " " + Describe());
}

// The load itself happens at the start of the next frame, then OnLoad continues.
void Demo_Load()
{
	if (!SaveExists())
	{
		Log("Nothing to load yet, F5 saves");
		Trace("SAVE: no save file");
		return;
	}
	Trace("SAVE: loading");
	LoadEngineState(SAVE_FILE);
}

// Where the ship is, where it heads and where the wind blows, in whole metres and degrees.
string Describe()
{
	int x = makeint(stf(Player.ship.pos.x));
	int z = makeint(stf(Player.ship.pos.z));
	return "position " + x + ", " + z + ", heading " + Degrees(stf(Player.ship.ang.y)) + ", wind " +
		Degrees(stf(Weather.Wind.Angle)) + ", sails " + makeint(sailState * 100.0) + "%";
}

int Degrees(float radians)
{
	int deg = makeint(radians * 57.29578);
	while (deg < 0) { deg = deg + 360; }
	while (deg >= 360) { deg = deg - 360; }
	return deg;
}

void Demo_ShipLayers()
{
	aref ship = GetEventData();
	SendMessage(ship, "lll", AI_MESSAGE_SET_LAYERS, EXECUTE, REALIZE);
}

// Top speeds of the ship, as in the ship demo; the sail state is kept for the log and the save.
void Demo_ShipParameters()
{
	int index = GetEventData();
	sailState = GetEventData();
	Demo_UpdateSpeeds(sailState);
}

void Demo_UpdateSpeeds(float sails)
{
	Player.Ship.MaxSpeedZ = stf(BaseShip.SpeedRate);
	Player.Ship.MaxSpeedY = stf(BaseShip.TurnRate) / 160.0 * (0.4 + 0.6 * sails);
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
	if (control == "Demo_Save") { Demo_Save(); }
	if (control == "Demo_Load") { Demo_Load(); }
	if (control == "Demo_WindLeft") { Demo_TurnWind(-0.5); }
	if (control == "Demo_WindRight") { Demo_TurnWind(0.5); }
	if (control == "Demo_Quit")
	{
		Trace("SAVE: quit");
		ExitProgram();
	}
}

void Demo_TurnWind(float delta)
{
	Weather.Wind.Angle = stf(Weather.Wind.Angle) + delta;
	Log("Wind " + Degrees(stf(Weather.Wind.Angle)));
}
