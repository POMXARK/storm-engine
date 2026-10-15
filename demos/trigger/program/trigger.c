// Trigger demo: a zone of a scene made in the editor and the script reacting to it. The sailor, an
// NPC, walks by himself between the markers "a" and "b" of resource/scenes/gate.scene; on the way he
// crosses the trigger "gate". The script tells the scene where he is (CheckSceneTriggers) and the
// scene sends SceneTriggerEnter and SceneTriggerExit as he crosses the border: on entering the guard
// takes his guard, the crate goes away and the bell rings, on leaving all comes back.
// No keys are needed; Escape quits. With --editor the zone is the green cylinder of the window Scene.

#define EXECUTE 0
#define REALIZE 1

#define MSG_LOCATION_ADD_MODEL 30100
#define MSG_LOCATION_SET_CHRS_PATCH 30120
#define MSG_LOCATION_MODELSPATH 30140
#define MSG_LOCATION_TEXTURESPATH 30141
#define MSG_CHARACTER_SETMODEL 30200
#define MSG_CHARACTER_GETPOS 30202
#define MSG_CHARACTER_TELEPORT_AY 30211
#define MSG_NPCHARACTER_SETTASK 30400
#define MSG_CAMERA_SETTARGET 30500
#define MSG_CAMERA_TOPOS 30512
#define LOG_ADD_STRING 45020
#define LOG_AND_ACTIONS_INIT 45022

#define TICK_MS 100
// the NPC stops about half a metre short of the point of his task
#define ARRIVED 1.0

// the scene module: declared before its libraries, which bind only what is declared
native int LoadScene(string name);
native int GetSceneMarker(string name, ref place);
native int SceneObjectAction(string name, string action);
native int SceneObjectShow(string name, int show);
native int SceneObjectMove(string name, float x, float y, float z, float ay);
native int CheckSceneTriggers(string who, float x, float y, float z);
#library "SceneLibrary"
#library "SceneMarkers"
#library "SceneObjects"

object Render;
object Scene;
object Weather;
object Yard;
object Groups;
object Sailor;
object Camera;
object ScreenLog;
object Target;

int shadowDensity[2];
string goal = "b";

void MapKey(string controlName, int keyCode)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
}

void Main()
{
	MapKey("ChrCamTurnV", 257);
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

	// the yard of the melee demo
	Yard.id = "yard";
	CreateEntity(&Yard, "location");
	SendMessage(&Yard, "ls", MSG_LOCATION_MODELSPATH, "yard");
	SendMessage(&Yard, "ls", MSG_LOCATION_TEXTURESPATH, "yard");
	if (!SendMessage(&Yard, "lssl", MSG_LOCATION_ADD_MODEL, "yard", "DLightModel", 10))
	{
		Trace("TRIGGER: yard model not loaded");
	}
	if (!SendMessage(&Yard, "ls", MSG_LOCATION_SET_CHRS_PATCH, "yard_patch"))
	{
		Trace("TRIGGER: character patch not loaded");
	}
	CreateEntity(&Groups, "CharactersGroups");

	SetEventHandler("EWhr_GetShadowDensity", "Demo_ShadowDensity", 0);
	SetEventHandler("EWhr_GetFogDensity", "Demo_FogDensity", 0);
	SetEventHandler("Control Activation", "Demo_Control", 0);
	SetEventHandler("SceneTriggerEnter", "Gate_Enter", 0);
	SetEventHandler("SceneTriggerExit", "Gate_Exit", 0);
	SetEventHandler("Demo_Tick", "Demo_Tick", 0);

	if (LoadScene("gate") < 0)
	{
		Trace("TRIGGER: scene gate not loaded");
	}
	// the bell of the scene rings only while someone is in the gate
	SceneObjectShow("bell", 0);

	Sailor.id = "sailor";
	CreateEntity(&Sailor, "npcharacter");
	if (!SendMessage(&Sailor, "lss", MSG_CHARACTER_SETMODEL, "sailor", "fencer"))
	{
		Trace("TRIGGER: sailor not loaded");
	}
	Sailor.actions.idle.i1 = "idle_1";
	Sailor.actions = "";
	PlaceOnMarker("a");

	CreateEntity(&Camera, "locationcamera");
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);
	SendMessage(&Camera, "li", MSG_CAMERA_SETTARGET, &Sailor);
	// the camera stands aside and turns after the sailor: the gate, the guard and the crate stay in view
	SendMessage(&Camera, "lfffl", MSG_CAMERA_TOPOS, -6.5, 3.2, -2.5, 1);

	shadowDensity[0] = argb(255, 96, 96, 96);
	shadowDensity[1] = argb(255, 96, 96, 64);

	CreateLog();
	Log("The sailor walks through the gate by himself");
	WalkTo("b");
	PostEvent("Demo_Tick", TICK_MS);
	Trace("TRIGGER: scene loaded");
}

// puts the sailor on a marker of the scene, facing along z
void PlaceOnMarker(string marker)
{
	object place;
	if (!GetSceneMarker(marker, &place))
	{
		Trace("TRIGGER: no marker " + marker);
		return;
	}
	SendMessage(&Sailor, "lffff", MSG_CHARACTER_TELEPORT_AY, stf(place.x), stf(place.y), stf(place.z), 0.0);
}

// sends the sailor walking to a marker of the scene
void WalkTo(string marker)
{
	goal = marker;
	if (!GetSceneMarker(marker, &Target))
	{
		Trace("TRIGGER: no marker " + marker);
		return;
	}
	SendMessage(&Sailor, "lsfff", MSG_NPCHARACTER_SETTASK, "Goto point", stf(Target.x), stf(Target.y), stf(Target.z));
}

// the zones do not watch anyone: every tick the script says where the sailor is
void Demo_Tick()
{
	float x = 0.0;
	float y = 0.0;
	float z = 0.0;
	float dx = 0.0;
	float dz = 0.0;
	SendMessage(&Sailor, "leee", MSG_CHARACTER_GETPOS, &x, &y, &z);
	CheckSceneTriggers("sailor", x, y, z);
	dx = x - stf(Target.x);
	dz = z - stf(Target.z);
	if (dx * dx + dz * dz < ARRIVED * ARRIVED)
	{
		if (goal == "a") { WalkTo("b"); }
		else { WalkTo("a"); }
	}
	PostEvent("Demo_Tick", TICK_MS);
}

void Gate_Enter()
{
	string trigger = GetEventData();
	string who = GetEventData();
	SceneObjectAction("guard", "fight stand");
	SceneObjectShow("crate", 0);
	SceneObjectShow("bell", 1);
	Log(who + " enters " + trigger);
	Trace("TRIGGER: " + who + " enters " + trigger);
}

void Gate_Exit()
{
	string trigger = GetEventData();
	string who = GetEventData();
	SceneObjectAction("guard", "idle_1");
	SceneObjectShow("crate", 1);
	SceneObjectShow("bell", 0);
	Log(who + " leaves " + trigger);
	Trace("TRIGGER: " + who + " leaves " + trigger);
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
	if (control == "Demo_Quit")
	{
		Trace("TRIGGER: quit");
		ExitProgram();
	}
}
