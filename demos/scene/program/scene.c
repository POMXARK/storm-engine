// Scene demo: what is in the scene is not in the script. The models and their places are in
// resource/scenes/harbour.scene, made in the window Scene of the editor (run.sh <engine> --editor);
// the script sets the camera, the sun and the fog and loads the scene by its name.
// Mouse looks around, W/S move the camera, Escape quits.

// LoadScene comes from the module scene of the engine: the function is declared as native, then its
// library of script functions is taken in by name (the library binds what is declared before it)
native int LoadScene(string name);
#library "SceneLibrary"

#define EXECUTE 0

object Render;
object Scene;
object Camera;
object Weather;

void MapKey(string controlName, int keyCode)
{
	int control = CreateControl(controlName);
	MapControl(control, keyCode);
}

void Main()
{
	MapKey("FreeCamera_Turn_H", 256);
	MapKey("FreeCamera_Turn_V", 257);
	MapKey("FreeCamera_Forward", 87);
	MapKey("FreeCamera_Backward", 83);
	MapKey("Demo_Quit", 27);

	Render.BackColor = argb(255, 120, 160, 200);

	Camera.position.x = 0.0;
	Camera.position.y = 2.2;
	Camera.position.z = -6.0;
	CreateEntity(&Camera, "FREE_CAMERA");
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);

	CreateEntity(&Weather, "weather");
	Weather.Fog.Enable = 1;
	Weather.Fog.Density = 0.02;
	Weather.Fog.Color = argb(0, 120, 160, 200);
	Weather.Sun.Color = argb(0, 255, 236, 200);
	Weather.Sun.Ambient = argb(0, 90, 100, 120);
	Weather.Sun.HeightAngle = 0.7;
	Weather.Sun.AzimuthAngle = 0.8;
	Weather.isDone = "";

	// the models of the scene go into the layers EXECUTE and REALIZE themselves
	int models = LoadScene("harbour");
	Trace("SCENE: harbour, " + models + " models");

	SetEventHandler("Control Activation", "Demo_Control", 0);
}

void Demo_Control()
{
	string control = GetEventData();
	if (control == "Demo_Quit")
	{
		ExitProgram();
	}
}
