// Sea demo: open sea under a skybox with three weathers. The "sea" entity draws animated waves that
// reflect the sky, "Sky" draws the skybox around the camera and "weather" owns the sun light and
// the fog. Mouse looks around, W/S (Shift, Ctrl to speed up) move the camera, 1/2/3 switch to day,
// sunset and storm, Escape quits.

#define EXECUTE 0
#define REALIZE 1
// entities in this layer are drawn again, mirrored, into the reflection the sea shows
#define SEA_REFLECTION 15

#define WEATHER_DAY 0
#define WEATHER_SUNSET 1
#define WEATHER_STORM 2

// the renderer reads Render.BackColor every frame
object Render;
object Scene;
object Camera;
object Weather;
object Sky;
object Sea;

// height of the fog band on the sky above the horizon, in sky units (Sky.Size is 2048)
float fogHeight = 150.0;

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
	MapKey("Demo_Day", 49);
	MapKey("Demo_Sunset", 50);
	MapKey("Demo_Storm", 51);
	MapKey("Demo_Quit", 27);

	Render.BackColor = argb(255, 0, 0, 0);

	// above the crests of the storm waves, looking along +z
	Camera.position.x = 0.0;
	Camera.position.y = 20.0;
	Camera.position.z = 0.0;
	CreateEntity(&Camera, "FREE_CAMERA");
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);

	CreateEntity(&Weather, "weather");

	// the sky builds a fog band over the horizon and asks the scripts its colour per vertex,
	// so the handler must be there before the sky is generated
	SetEventHandler("WhrCalcFogColor", "Demo_FogColor", 0);
	CreateEntity(&Sky, "Sky");
	Sky.Size = 2048.0;
	Sky.Angle = 0.0;
	Sky.RotateSpeed = 0.0;
	// the techniques of resource\techniques\weather\Sky.fx for the sky at sea
	Sky.techSky = "Sky";
	Sky.techSkyBlend = "SkyBlend";
	Sky.techSkyAlpha = "skyblend_alpha";
	Sky.techSkyFog = "SkyFog";
	LayerAddObject(EXECUTE, &Sky, 2);
	LayerAddObject(REALIZE, &Sky, 3);
	LayerAddObject(SEA_REFLECTION, &Sky, 1);

	// loads resource\sea\sea0000..0063.tga, the wave heightmaps; drawn last, over the sky
	CreateEntity(&Sea, "sea");
	LayerAddObject(EXECUTE, &Sea, 4);
	LayerAddObject(REALIZE, &Sea, 65530);

	SetWeather(WEATHER_DAY);

	SetEventHandler("Control Activation", "Demo_Control", 0);
	Trace("SEA: scene loaded");
}

// Applies one of the presets to the weather, the sky and the sea. The sun angles match the sun
// painted on the skies by make_sea.py.
void SetWeather(int preset)
{
	string skyDir = "weather\skies\day\";
	int fogColor = argb(0, 150, 185, 215);
	float fogDensity = 0.002;
	int sunColor = argb(0, 230, 220, 205);
	int sunAmbient = argb(0, 95, 100, 105);
	float sunHeight = 0.7;
	float sunAzimuth = 2.618;
	int waterColor = argb(0, 30, 90, 115);
	int skyColor = argb(0, 205, 255, 255);
	float amp1 = 6.0;
	float scale1 = 0.9;
	float amp2 = 1.1;
	float scale2 = 3.8;
	float foamHeight = 2.5;

	if (preset == WEATHER_SUNSET)
	{
		skyDir = "weather\skies\sunset\";
		fogColor = argb(0, 200, 130, 90);
		fogDensity = 0.0025;
		sunColor = argb(0, 255, 170, 110);
		sunAmbient = argb(0, 70, 60, 80);
		sunHeight = 0.06;
		sunAzimuth = 3.491;
		waterColor = argb(0, 40, 50, 80);
		skyColor = argb(0, 255, 200, 160);
		amp1 = 4.0;
		amp2 = 0.8;
	}
	if (preset == WEATHER_STORM)
	{
		skyDir = "weather\skies\storm\";
		fogColor = argb(0, 95, 102, 110);
		fogDensity = 0.006;
		sunColor = argb(0, 90, 90, 95);
		sunAmbient = argb(0, 45, 48, 52);
		sunHeight = 0.5;
		sunAzimuth = 2.618;
		waterColor = argb(0, 45, 60, 70);
		skyColor = argb(0, 150, 155, 160);
		amp1 = 12.0;
		scale1 = 0.3;
		amp2 = 2.0;
		scale2 = 1.6;
		foamHeight = 6.0;
	}

	// weather: the sun is a directional light, set on isDone together with the ambient and fog
	Weather.Fog.Enable = 1;
	Weather.Fog.Start = 0.0;
	Weather.Fog.Density = fogDensity;
	Weather.Fog.Color = fogColor;
	Weather.Sun.Color = sunColor;
	Weather.Sun.Ambient = sunAmbient;
	Weather.Sun.HeightAngle = sunHeight;
	Weather.Sun.AzimuthAngle = sunAzimuth;
	Weather.isDone = "";

	// sky: Dir.dN are skybox folders blended over the day by the value of Dir (an hour); one
	// folder is a static sky. isDone reloads the five faces sky_fr/lf/bk/rt/up.tga from it
	DeleteAttribute(&Sky, "Dir");
	Sky.Dir.d0 = skyDir;
	Sky.Dir = "12";
	Sky.Color = argb(0, 255, 255, 255);
	Sky.isDone = "";

	// sea: two layers of the same wave animation, a large slow one (1) and small fast ripples (2),
	// scaled, animated and scrolled separately; the colour mixes the water and the reflected sky
	Sea.Sea2.WaterColor = waterColor;
	Sea.Sea2.SkyColor = skyColor;
	Sea.Sea2.Amp1 = amp1;
	Sea.Sea2.AnimSpeed1 = 1.5;
	Sea.Sea2.Scale1 = scale1;
	Sea.Sea2.MoveSpeed1 = "2.0, 0.0, 0.0";
	Sea.Sea2.Amp2 = amp2;
	Sea.Sea2.AnimSpeed2 = 15.0;
	Sea.Sea2.Scale2 = scale2;
	Sea.Sea2.MoveSpeed2 = "1.0, 0.0, 1.0";
	Sea.Sea2.PosShift = 1.0;
	Sea.Sea2.BumpScale = 0.05;
	Sea.Sea2.Reflection = 0.6;
	Sea.Sea2.Transparency = 0.45;
	Sea.Sea2.Frenel = 0.45;
	Sea.Sea2.Attenuation = 0.3;
	Sea.Sea2.LodScale = 0.5;
	Sea.Sea2.GridStep = 0.1;
	// foam texture weather\sea\pena\pena.tga on crests above FoamV metres
	Sea.Sea2.FoamEnable = 1;
	Sea.Sea2.FoamK = 0.2;
	Sea.Sea2.FoamV = foamHeight;
	Sea.Sea2.FoamUV = 0.5;
	Sea.Sea2.FoamTexDisturb = 0.7;
	Sea.Sea2.SimpleSea = 0;
	Sea.Fog.Enable = 1;
	Sea.Fog.Start = 0.0;
	Sea.Fog.Color = fogColor;
	Sea.Fog.SeaDensity = fogDensity * 0.5;
	// waves are scaled down so that both layers together stay under this height
	Sea.MaxSeaHeight = 50.0;
	Sea.isDone = "";

	Trace("SEA: weather " + skyDir);
}

// Colour of the fog band of the sky at a point: the fog colour, opaque at the horizon and fading
// out at fogHeight.
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
	if (control == "Demo_Day") { SetWeather(WEATHER_DAY); }
	if (control == "Demo_Sunset") { SetWeather(WEATHER_SUNSET); }
	if (control == "Demo_Storm") { SetWeather(WEATHER_STORM); }
	if (control == "Demo_Quit")
	{
		Trace("SEA: quit");
		ExitProgram();
	}
}
