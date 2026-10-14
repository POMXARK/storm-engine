// Location demo: a walkable yard. The "location" entity loads the yard model and its character
// patch, the "player" character walks on the patch driven by the controls, and "locationcamera"
// follows him. W/S walk forward/back, A/D or the mouse turn, holding Shift walks instead of running,
// the mouse also tilts the camera, P writes the sailor's position to the log, Escape quits.

#define EXECUTE 0
#define REALIZE 1

#define MSG_LOCATION_ADD_MODEL 30100
#define MSG_LOCATION_SET_CHRS_PATCH 30120
#define MSG_LOCATION_MODELSPATH 30140
#define MSG_LOCATION_TEXTURESPATH 30141
#define MSG_CHARACTER_SETMODEL 30200
#define MSG_CHARACTER_GETPOS 30202
#define MSG_CHARACTER_GETAY 30203
#define MSG_CHARACTER_TELEPORT_AY 30211
#define MSG_CAMERA_SETTARGET 30500
#define MSG_CAMERA_FOLLOW 30510

// the renderer reads Render.BackColor every frame
object Render;
object Scene;
object Weather;
object Yard;
object Groups;
object Sailor;
object Camera;

// head and foot colours of the characters' shadows, answered to the shadow entity
int shadowDensity[2];

void MapKey(string controlName, int keyCode)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
}

void Main()
{
	// the controls the player character and the location camera read;
	// key codes are Windows virtual keys, 256/257 are the mouse axes
	MapKey("ChrForward", 87);
	MapKey("ChrBackward", 83);
	MapKey("ChrTurnHL", 65);
	MapKey("ChrTurnHR", 68);
	// running is the default, the key switches to walking
	MapKey("ChrRun", 16);
	MapKey("ChrTurnH", 256);
	MapKey("ChrCamTurnV", 257);
	MapKey("Demo_Where", 80);
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

	// the location runs the characters in it: they need it before they are created
	Yard.id = "yard";
	CreateEntity(&Yard, "location");
	// models and the patch come from resource\models\yard\, textures from resource\textures\yard\
	SendMessage(&Yard, "ls", MSG_LOCATION_MODELSPATH, "yard");
	SendMessage(&Yard, "ls", MSG_LOCATION_TEXTURESPATH, "yard");
	if (!SendMessage(&Yard, "lssl", MSG_LOCATION_ADD_MODEL, "yard", "DLightModel", 10))
	{
		Trace("LOCATION: yard model not loaded");
	}
	// the floor characters walk on: triangles, their neighbours and the path table
	if (!SendMessage(&Yard, "ls", MSG_LOCATION_SET_CHRS_PATCH, "yard_patch"))
	{
		Trace("LOCATION: character patch not loaded");
	}
	// characters register in a group (friends, enemies) when created
	CreateEntity(&Groups, "CharactersGroups");

	Sailor.id = "sailor";
	CreateEntity(&Sailor, "player");
	// resource\models\characters\sailor.gm with resource\animation\sailor.ani; the actions the
	// character plays (walk, run, turn left...) are looked up by name in the .ani
	if (!SendMessage(&Sailor, "lss", MSG_CHARACTER_SETMODEL, "sailor", "sailor"))
	{
		Trace("LOCATION: sailor not loaded");
	}
	// the sailor has one standing action instead of the default idle_1..idle_11; changing
	// "actions" makes the character read the action names again
	Sailor.actions.idle.i1 = "idle_1";
	Sailor.actions = "";
	// stand on the patch near the south wall, facing north (+z)
	SendMessage(&Sailor, "lffff", MSG_CHARACTER_TELEPORT_AY, 0.0, 0.0, -5.0, 0.0);

	CreateEntity(&Camera, "locationcamera");
	SendMessage(&Camera, "li", MSG_CAMERA_SETTARGET, &Sailor);
	SendMessage(&Camera, "l", MSG_CAMERA_FOLLOW);
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);

	// every character's shadow asks the weather scripts for its density and the fog each frame
	shadowDensity[0] = argb(255, 96, 96, 96);
	shadowDensity[1] = argb(255, 96, 96, 64);
	SetEventHandler("EWhr_GetShadowDensity", "Demo_ShadowDensity", 0);
	SetEventHandler("EWhr_GetFogDensity", "Demo_FogDensity", 0);
	SetEventHandler("Control Activation", "Demo_Control", 0);
	Trace("LOCATION: scene loaded");
}

ref Demo_ShadowDensity()
{
	return &shadowDensity;
}

float Demo_FogDensity()
{
	return stf(Weather.Fog.Density);
}

void Demo_Control()
{
	string control = GetEventData();
	if (control == "Demo_Where")
	{
		float x, y, z, ay;
		SendMessage(&Sailor, "leee", MSG_CHARACTER_GETPOS, &x, &y, &z);
		SendMessage(&Sailor, "le", MSG_CHARACTER_GETAY, &ay);
		Trace("LOCATION: sailor at " + x + ", " + y + ", " + z + " facing " + ay);
	}
	if (control == "Demo_Quit")
	{
		Trace("LOCATION: quit");
		ExitProgram();
	}
}
