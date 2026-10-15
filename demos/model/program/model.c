// Model demo: static .gm models placed by script, a free camera and dynamic sun lighting.
// Mouse looks around, W/S (Shift, Ctrl to speed up) move the camera, Escape quits.

#define EXECUTE 0
#define REALIZE 1

#define MSG_MODEL_LOAD_GEO 20500
#define MSG_MODEL_SET_POSITION 20506
#define MSG_MODEL_SET_TECHNIQUE 20510

// the renderer reads Render.BackColor every frame
object Render;
object Scene;
object Camera;
object Weather;
object Deck;
object Barrel1;
object Barrel2;
object Crate1;
object Crate2;

float sunAngle = 0.0;
float spinAngle = 0.0;

void MapKey(string controlName, int keyCode, int flags)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
	if (flags != 0)
	{
		SetControlFlags(control, flags);
	}
}

void Main()
{
	// key codes are Windows virtual keys, 256/257 are the mouse axes
	MapKey("FreeCamera_Turn_H", 256, 0);
	MapKey("FreeCamera_Turn_V", 257, 0);
	MapKey("FreeCamera_Forward", 87, 0);
	MapKey("FreeCamera_Backward", 83, 0);
	MapKey("Demo_Quit", 27, 0);

	Render.BackColor = argb(255, 120, 160, 200);

	// the camera starts at the given point looking along +z
	Camera.position.x = 0.0;
	Camera.position.y = 1.6;
	Camera.position.z = -4.5;
	CreateEntity(&Camera, "FREE_CAMERA");
	// cameras are not run by layers: the Scene entity executes the active one
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);

	// WEATHER owns the sun: a directional D3D light plus the ambient colour, applied on isDone
	CreateEntity(&Weather, "weather");
	// exponential fog in the sky colour hides the edge of the deck
	Weather.Fog.Enable = 1;
	Weather.Fog.Density = 0.025;
	Weather.Fog.Color = argb(0, 120, 160, 200);
	Weather.Sun.Color = argb(0, 255, 236, 200);
	Weather.Sun.Ambient = argb(0, 90, 100, 120);
	Weather.Sun.HeightAngle = 0.7;
	Weather.Sun.AzimuthAngle = sunAngle;
	Weather.isDone = "";

	LoadModel(&Deck, "demo\deck");
	LoadModel(&Barrel1, "demo\barrel");
	LoadModel(&Barrel2, "demo\barrel");
	LoadModel(&Crate1, "demo\crate");
	LoadModel(&Crate2, "demo\crate");
	PlaceModel(&Deck, 0.0, 0.0, 0.0, 0.0);
	PlaceModel(&Barrel2, 1.3, 0.0, 0.8, 0.0);
	PlaceModel(&Crate1, -1.4, 0.0, 0.4, 0.3);
	PlaceModel(&Crate2, -1.3, 0.8, 0.4, 0.9);

	SetEventHandler("frame", "Demo_Frame", 0);
	SetEventHandler("Control Activation", "Demo_Control", 0);
	Trace("MODEL: scene loaded");
}

void LoadModel(ref model, string name)
{
	CreateEntity(&model, "modelr");
	SendMessage(&model, "ls", MSG_MODEL_LOAD_GEO, name);
	// fixed-function lighting by the D3D lights; without a technique vertex colours are drawn as is
	SendMessage(&model, "ls", MSG_MODEL_SET_TECHNIQUE, "DLightModel");
	LayerAddObject(EXECUTE, &model, 10);
	LayerAddObject(REALIZE, &model, 10);
}

// position and rotation around the vertical axis
void PlaceModel(ref model, float x, float y, float z, float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	SendMessage(&model, "lffffffffffff", MSG_MODEL_SET_POSITION, x, y, z, c, 0.0, 0.0 - s, 0.0, 1.0, 0.0, s, 0.0, c);
}

void Demo_Frame()
{
	float dt = GetDeltaTime() * 0.001;

	spinAngle = spinAngle + dt * SpinSpeed();
	PlaceModel(&Barrel1, 0.0, 0.0, 0.0, spinAngle);

	// the sun circles the scene, so the shading visibly follows it
	sunAngle = sunAngle + dt * 0.4;
	Weather.Sun.AzimuthAngle = sunAngle;
	Weather.isDone = "";
}

void Demo_Control()
{
	string control = GetEventData();
	if (control == "Demo_Quit")
	{
		Trace("MODEL: quit");
		ExitProgram();
	}
}

// how fast Barrel1 turns, radians a second. Edit it in the editor (window Assets): Ctrl+S saves the
// script and compiles it again, the barrel goes on turning from spinAngle at the new speed
float SpinSpeed()
{
	return 0.8;
}
