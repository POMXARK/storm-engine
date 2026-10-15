// World map demo: the WORLDMAP entity sailing an archipelago of four islands. The entity runs the
// whole map itself - the sea, the islands and their names, the player's ship and its wake, the
// clouds, the clock with the sky of the day and the wind, the compass - and asks the script only
// about encounters: every second it sends WorldMap_EncounterCreate, and the script answers with
// messages that put merchants, pursuers, fighting pairs and storms on the map. The player's ship
// reports what it meets: WorldMap_ShipEncounter when an enemy catches it (or it boards one with
// Enter), WorldMap_PlayerInStorm inside a storm, ExitFromWorldMap when Enter leaves the map with
// no ship near; worldMap.island names the island whose waters the ship is in.
// Until a key takes the helm the ship tours the islands by itself: worldMap.autopilot.x / z sends
// it to a point, and the engine deletes the attribute when it is there.
// W / S raise and lower the sails, A / D turn, the mouse turns the camera, the wheel zooms,
// Ctrl turns the camera round the ship, Tab switches the camera, Escape quits.

#define EXECUTE 0
#define REALIZE 1

#define LOG_ADD_STRING 45020
#define LOG_AND_ACTIONS_INIT 45022

// shared/messages.h
#define MSG_WORLDMAP_CREATESTORM 31000
#define MSG_WORLDMAP_CREATEENC_MER_XZ 31102
#define MSG_WORLDMAP_CREATEENC_FLW 31110
#define MSG_WORLDMAP_CREATEENC_WAR 31120
#define MSG_WORLDMAP_CREATEENC_RELEASE 31200

// a new encounter at most this often, seconds; and how many may be on the map at once
#define ENCOUNTER_PERIOD 12.0
#define ENCOUNTER_LIFE 90.0
#define ENCOUNTER_MAX 4
#define STORM_PERIOD 60.0

object worldMap;
object ScreenLog;

float encounterTimer = 8.0;
float stormTimer = 20.0;
int encounterCount = 0;
float encounterQuiet = 0.0;
float stormLogTimer = 0.0;
string lastIsland = "";
int encounterTurn = 0;
// the tour: a point off the shore of every island, in turn; -1 when the player has the helm
int tourStep = 0;
float tourX[4] = {-250.0, 280.0, 120.0, -260.0};
float tourZ[4] = {250.0, 380.0, -280.0, -400.0};

void MapKey(string controlName, int keyCode)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
}

void Main()
{
	MapKey("WMapShipSailUp", 87);
	MapKey("WMapShipSailDown", 83);
	MapKey("WMapShipTurnLeft", 65);
	MapKey("WMapShipTurnRight", 68);
	MapKey("WMapTurnH", 256);
	MapKey("WMapTurnV", 257);
	MapKey("WMapCameraRotate", 17);
	MapKey("WMapForward", 258);
	MapKey("WMapBackward", 259);
	MapKey("WMapCameraSwitch", 9);
	MapKey("WMapCancel", 13);
	MapKey("Demo_Quit", 27);

	CreateLog();
	CreateWorldMap();

	SetEventHandler("Control Activation", "Demo_Control", 0);
	SetEventHandler("WorldMap_EncounterCreate", "Demo_EncounterCreate", 0);
	SetEventHandler("WorldMap_ShipEncounter", "Demo_ShipEncounter", 0);
	SetEventHandler("WorldMap_PlayerInStorm", "Demo_PlayerInStorm", 0);
	SetEventHandler("ExitFromWorldMap", "Demo_ExitFromMap", 0);
	SetEventHandler("WorldMap_GetMoral", "Demo_GetMorale", 0);
	SetEventHandler("WorldMap_GetFood", "Demo_GetFood", 0);
	SetEventHandler("WorldMap_GetRum", "Demo_GetRum", 0);
	Log("The ship tours the islands; W / S / A / D take the helm. Merchants, pirates and storms come with the wind");
	TourNext();
	Trace("WORLDMAP: map loaded");
}

void CreateWorldMap()
{
	// the player starts between the islands, bow to the north
	worldMap.playerShipX = 0.0;
	worldMap.playerShipZ = -60.0;
	worldMap.playerShipAY = 0.0;
	worldMap.playerShipActionRadius = 20.0;
	worldMap.wdmCameraY = 250.0;
	worldMap.wdmCameraAY = 0.0;
	// enemies and storms are seen from afar and fade out past these distances
	worldMap.enemyshipViewDistMin = 300.0;
	worldMap.enemyshipViewDistMax = 500.0;
	worldMap.enemyshipDistKill = 800.0;
	worldMap.enemyshipBrnDistMin = 250.0;
	worldMap.enemyshipBrnDistMax = 400.0;
	worldMap.stormViewDistMin = 300.0;
	worldMap.stormViewDistMax = 500.0;
	worldMap.stormDistKill = 900.0;
	worldMap.stormBrnDistMin = 300.0;
	worldMap.stormBrnDistMax = 450.0;
	worldMap.stormZone = 90.0;

	// a game hour passes in two seconds of the map
	worldMap.date.hour = 9;
	worldMap.date.min = 0;
	worldMap.date.day = 15;
	worldMap.date.month = 6;
	worldMap.date.year = 1665;
	worldMap.date.hourPerSec = 0.5;
	worldMap.date.font = "WDM_DATE";

	// the names of the islands, over the label points of the base model
	AddLabel("gull", "Gull Island");
	AddLabel("crag", "The Crag");
	AddLabel("palm", "Palm Key");
	AddLabel("reef", "Reef Cay");

	CreateEntity(&worldMap, "worldmap");
}

void AddLabel(string id, string text)
{
	string attr = "labels." + id;
	worldMap.(attr).id = id;
	worldMap.(attr).locator = id;
	worldMap.(attr).text = text;
	worldMap.(attr).font = "WDM_LABEL";
	worldMap.(attr).heightView = 400.0;
}

// every second: the time since the last call and where the player is
void Demo_EncounterCreate()
{
	float dltTime = GetEventData();
	float x = GetEventData();
	float z = GetEventData();
	float ay = GetEventData();

	if (tourStep >= 0 && !CheckAttribute(&worldMap, "autopilot")) { TourNext(); }
	if (encounterQuiet > 0.0) { encounterQuiet = encounterQuiet - dltTime; }
	if (stormLogTimer > 0.0) { stormLogTimer = stormLogTimer - dltTime; }

	string island = worldMap.island;
	if (island != lastIsland)
	{
		if (island != "") { Log("In the waters of " + IslandName(island)); }
		lastIsland = island;
	}

	stormTimer = stormTimer - dltTime;
	if (stormTimer <= 0.0)
	{
		stormTimer = STORM_PERIOD;
		SendMessage(&worldMap, "ll", MSG_WORLDMAP_CREATESTORM, false);
		Log("A storm is gathering");
	}

	encounterTimer = encounterTimer - dltTime;
	if (encounterTimer > 0.0 || encounterCount >= ENCOUNTER_MAX || encounterQuiet > 0.0) { return; }
	encounterTimer = ENCOUNTER_PERIOD;
	encounterCount = encounterCount + 1;

	// in turn: a merchant crossing ahead, a pirate that hunts the player, two ships fighting
	if (encounterTurn == 0)
	{
		float a = ay + (frnd() - 0.5) * 2.0;
		float x1 = x + sin(a) * 300.0 - cos(a) * 250.0;
		float z1 = z + cos(a) * 300.0 + sin(a) * 250.0;
		float x2 = x + sin(a) * 300.0 + cos(a) * 250.0;
		float z2 = z + cos(a) * 300.0 - sin(a) * 250.0;
		SendMessage(&worldMap, "lsffffff", MSG_WORLDMAP_CREATEENC_MER_XZ, "merchant", x1, z1, x2, z2, 1.0, ENCOUNTER_LIFE);
		Log("A merchant sails across our course");
	}
	if (encounterTurn == 1)
	{
		SendMessage(&worldMap, "lsff", MSG_WORLDMAP_CREATEENC_FLW, "pirate", 0.9, ENCOUNTER_LIFE);
		Log("Black sails on the horizon: a pirate has seen us");
	}
	if (encounterTurn == 2)
	{
		SendMessage(&worldMap, "lssf", MSG_WORLDMAP_CREATEENC_WAR, "pirate", "merchant", ENCOUNTER_LIFE);
		Log("Gunfire: a pirate attacks a merchant");
	}
	encounterTurn = encounterTurn + 1;
	if (encounterTurn > 2) { encounterTurn = 0; }
}

// an enemy caught the player (or Enter boarded the one in reach): in the game the sea battle
// would start here; the demo clears the map and goes on
void Demo_ShipEncounter()
{
	float x = GetEventData();
	float z = GetEventData();
	float ay = GetEventData();
	int index = GetEventData();
	if (encounterQuiet > 0.0) { return; }
	Log("Encounter with ship " + index + " at " + MakeInt(x) + ", " + MakeInt(z) + ": here the battle at sea would begin");
	Trace("WORLDMAP: ship encounter " + index);
	SendMessage(&worldMap, "l", MSG_WORLDMAP_CREATEENC_RELEASE);
	encounterCount = 0;
	encounterQuiet = 10.0;
	encounterTimer = ENCOUNTER_PERIOD;
}

void Demo_PlayerInStorm()
{
	float x = GetEventData();
	float z = GetEventData();
	float ay = GetEventData();
	int index = GetEventData();
	if (stormLogTimer > 0.0) { return; }
	stormLogTimer = 5.0;
	Log("Caught in the storm!");
	Trace("WORLDMAP: in storm " + index);
}

void Demo_ExitFromMap()
{
	string island = worldMap.island;
	if (island == "") { Log("Out to the open sea"); }
	else { Log("Out to sea at " + IslandName(island)); }
	Trace("WORLDMAP: exit to sea at '" + island + "'");
}

float Demo_GetMorale() { return 70.0; }
float Demo_GetFood() { return 42.0; }
float Demo_GetRum() { return 0.0; }

string IslandName(string id)
{
	string attr = "labels." + id;
	if (!CheckAttribute(&worldMap, attr)) { return id; }
	return worldMap.(attr).text;
}

void TourNext()
{
	worldMap.autopilot.x = tourX[tourStep];
	worldMap.autopilot.z = tourZ[tourStep];
	worldMap.autopilot.radius = 15.0;
	Trace("WORLDMAP: tour to " + tourStep);
	tourStep = tourStep + 1;
	if (tourStep > 3) { tourStep = 0; }
}

void Demo_Control()
{
	string control = GetEventData();
	bool helm = control == "WMapShipSailUp" || control == "WMapShipSailDown";
	if (control == "WMapShipTurnLeft" || control == "WMapShipTurnRight") { helm = true; }
	if (helm && tourStep >= 0)
	{
		tourStep = -1;
		DeleteAttribute(&worldMap, "autopilot");
		Log("The helm is yours");
	}
	if (control == "Demo_Quit")
	{
		Trace("WORLDMAP: quit");
		ExitProgram();
	}
}

void CreateLog()
{
	ScreenLog.Log.left = 16;
	ScreenLog.Log.up = 12;
	ScreenLog.Log.width = 600;
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
