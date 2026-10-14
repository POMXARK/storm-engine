// Character demo: a skinned .gm model driven by a skeletal animation (.an + .ani actions).
// Space switches the sailor between "idle" and walking in a circle; mouse looks around,
// W/S (Shift, Ctrl to speed up) move the camera, Escape quits.

#define EXECUTE 0
#define REALIZE 1

#define MSG_MODEL_LOAD_GEO 20500
#define MSG_MODEL_LOAD_ANI 20501
#define MSG_MODEL_SET_POSITION 20506
#define MSG_MODEL_SET_TECHNIQUE 20510
#define MSG_MODEL_PLAY_ACTION 20601

// metres per second covered by one "walk" cycle, printed by make_character.py
#define WALK_SPEED 1.312
#define CIRCLE_X 0.0
#define CIRCLE_Z 1.5
#define CIRCLE_RADIUS 2.0

// the renderer reads Render.BackColor every frame
object Render;
object Scene;
object Camera;
object Weather;
object Floor;
object Sailor;

int walking = 0;
// the sailor stands at CIRCLE + RADIUS * (sin, cos) of this angle
float circleAngle = 0.0;

void MapKey(string controlName, int keyCode)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
}

void Main()
{
	// key codes are Windows virtual keys, 256/257 are the mouse axes
	MapKey("FreeCamera_Turn_H", 256);
	MapKey("FreeCamera_Turn_V", 257);
	MapKey("FreeCamera_Forward", 87);
	MapKey("FreeCamera_Backward", 83);
	MapKey("Demo_Walk", 32);
	MapKey("Demo_Quit", 27);

	Render.BackColor = argb(255, 120, 160, 200);

	Camera.position.x = 0.0;
	Camera.position.y = 1.5;
	Camera.position.z = -3.5;
	CreateEntity(&Camera, "FREE_CAMERA");
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);

	CreateEntity(&Weather, "weather");
	Weather.Fog.Enable = 1;
	Weather.Fog.Density = 0.03;
	Weather.Fog.Color = argb(0, 120, 160, 200);
	Weather.Sun.Color = argb(0, 255, 236, 200);
	Weather.Sun.Ambient = argb(0, 90, 100, 120);
	Weather.Sun.HeightAngle = 0.8;
	Weather.Sun.AzimuthAngle = 2.5;
	Weather.isDone = "";

	LoadModel(&Floor, "demo\floor");
	PlaceModel(&Floor, 0.0, 0.0, 0.0, 0.0);

	LoadModel(&Sailor, "demo\sailor");
	// the skeleton and its actions: resource\animation\demo\sailor.ani, which names the .an file;
	// a model with an animation is skinned by its bones every frame
	SendMessage(&Sailor, "ls", MSG_MODEL_LOAD_ANI, "demo\sailor");
	// player 0 of the animation; an action loops if its section says loop = true
	SendMessage(&Sailor, "lls", MSG_MODEL_PLAY_ACTION, 0, "idle");
	PlaceSailor();

	SetEventHandler("frame", "Demo_Frame", 0);
	SetEventHandler("Control Activation", "Demo_Control", 0);
	Trace("CHARACTER: scene loaded");
}

void LoadModel(ref model, string name)
{
	CreateEntity(&model, "modelr");
	SendMessage(&model, "ls", MSG_MODEL_LOAD_GEO, name);
	SendMessage(&model, "ls", MSG_MODEL_SET_TECHNIQUE, "DLightModel");
	LayerAddObject(EXECUTE, &model, 10);
	LayerAddObject(REALIZE, &model, 10);
}

// position and rotation around the vertical axis; the model faces +z at angle 0
void PlaceModel(ref model, float x, float y, float z, float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	SendMessage(&model, "lffffffffffff", MSG_MODEL_SET_POSITION, x, y, z, c, 0.0, 0.0 - s, 0.0, 1.0, 0.0, s, 0.0, c);
}

void PlaceSailor()
{
	float x = CIRCLE_X + CIRCLE_RADIUS * sin(circleAngle);
	float z = CIRCLE_Z + CIRCLE_RADIUS * cos(circleAngle);
	// walking with a growing angle goes along (cos, -sin): facing angle + pi/2
	PlaceModel(&Sailor, x, 0.0, z, circleAngle + 1.5708);
}

void Demo_Frame()
{
	if (walking)
	{
		float dt = GetDeltaTime() * 0.001;
		// the feet do not slide when the model moves as fast as the animation steps
		circleAngle = circleAngle + dt * WALK_SPEED / CIRCLE_RADIUS;
		PlaceSailor();
	}
}

void Demo_Control()
{
	string control = GetEventData();
	if (control == "Demo_Walk")
	{
		walking = 1 - walking;
		if (walking)
		{
			SendMessage(&Sailor, "lls", MSG_MODEL_PLAY_ACTION, 0, "walk");
		}
		else
		{
			SendMessage(&Sailor, "lls", MSG_MODEL_PLAY_ACTION, 0, "idle");
		}
		Trace("CHARACTER: walking " + walking);
	}
	if (control == "Demo_Quit")
	{
		Trace("CHARACTER: quit");
		ExitProgram();
	}
}
