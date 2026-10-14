// Particles demo: a fire burning on a rock in the sea, cannonball splashes and powder blasts on the
// water around it. The "particles" entity creates particle systems from resource\particles\*.xps
// (written by make_particles.py) and draws them with the sprites of
// resource\textures\particles_list.tga. Mouse looks around, W/S move the camera, 1 drops a
// cannonball into the sea, 2 sets off a blast, F puts the fire out and lights it again, Escape
// quits.

#define EXECUTE 0
#define REALIZE 1
// entities in this layer are drawn again, mirrored, into the reflection the sea shows
#define SEA_REFLECTION 15

#define MSG_MODEL_LOAD_GEO 20500
#define MSG_MODEL_SET_POSITION 20506
#define MSG_MODEL_SET_TECHNIQUE 20510

// "lsffffffl": name, position, angles, lifetime in ms (0 burns until deleted)
#define PS_CREATE 10000
// systems created between these two are remembered by the entity, PS_CLEAR_CAPTURED deletes them;
// PS_CREATE also returns the system, but as a 64-bit pointer a script int cannot hold
#define PS_CREATIONCAPTURE_BEG 10011
#define PS_CREATIONCAPTURE_END 10012
#define PS_CLEAR_CAPTURED 10013

// the top of the rock, where the fire burns
#define ROCK_TOP 2.5

object Render;
object Scene;
object Camera;
object Weather;
object Sky;
object Sea;
object Rock;
object Particles;

int fireBurning = 0;

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
	MapKey("Demo_Splash", 49);
	MapKey("Demo_Explosion", 50);
	MapKey("Demo_Fire", 70);
	MapKey("Demo_Quit", 27);

	Render.BackColor = argb(255, 0, 0, 0);

	// in front of the rock, looking at it along +z
	Camera.position.x = 0.0;
	Camera.position.y = 5.0;
	Camera.position.z = -24.0;
	CreateEntity(&Camera, "FREE_CAMERA");
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);

	CreateWorld();

	CreateEntity(&Rock, "modelr");
	SendMessage(&Rock, "ls", MSG_MODEL_LOAD_GEO, "rock\rock");
	SendMessage(&Rock, "ls", MSG_MODEL_SET_TECHNIQUE, "DLightModel");
	SendMessage(&Rock, "lffffffffffff", MSG_MODEL_SET_POSITION, 0.0, ROCK_TOP, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
	LayerAddObject(EXECUTE, &Rock, 10);
	LayerAddObject(REALIZE, &Rock, 10);
	LayerAddObject(SEA_REFLECTION, &Rock, 10);

	// the particle service has loaded every resource\particles\*.xps at start; the entity creates
	// systems from them and draws them after everything else
	CreateEntity(&Particles, "particles");
	LightFire();

	SetEventHandler("Control Activation", "Demo_Control", 0);
	Trace("PARTICLES: scene loaded");
}

// Sea, sky and weather as in the sea demo, with the day preset and a calmer sea.
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
	Sky.Dir.d0 = "weather\skies\day\";
	Sky.Dir = "12";
	Sky.Color = argb(0, 255, 255, 255);
	LayerAddObject(EXECUTE, &Sky, 2);
	LayerAddObject(REALIZE, &Sky, 3);
	LayerAddObject(SEA_REFLECTION, &Sky, 1);
	Sky.isDone = "";

	CreateEntity(&Sea, "sea");
	LayerAddObject(EXECUTE, &Sea, 4);
	LayerAddObject(REALIZE, &Sea, 65530);
	Sea.Sea2.WaterColor = argb(0, 30, 90, 115);
	Sea.Sea2.SkyColor = argb(0, 205, 255, 255);
	Sea.Sea2.Amp1 = 2.0;
	Sea.Sea2.AnimSpeed1 = 1.5;
	Sea.Sea2.Scale1 = 0.9;
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
	Sea.Sea2.FoamV = 2.5;
	Sea.Sea2.FoamUV = 0.5;
	Sea.Sea2.FoamTexDisturb = 0.7;
	Sea.Sea2.SimpleSea = 0;
	Sea.Fog.Enable = 1;
	Sea.Fog.Start = 0.0;
	Sea.Fog.Color = Weather.Fog.Color;
	Sea.Fog.SeaDensity = 0.001;
	Sea.MaxSeaHeight = 50.0;
	Sea.isDone = "";
}

// A looping system lives until it is deleted, so the fire is created under capture.
void LightFire()
{
	SendMessage(&Particles, "l", PS_CREATIONCAPTURE_BEG);
	SendMessage(&Particles, "lsffffffl", PS_CREATE, "campfire", 0.0, ROCK_TOP, 0.0, 0.0, 0.0, 0.0, 0);
	SendMessage(&Particles, "l", PS_CREATIONCAPTURE_END);
	fireBurning = 1;
	Trace("PARTICLES: fire lit");
}

// One-shot systems stop emitting by themselves and are removed once their last particle dies.
// They go off on the water at a random point between the camera and the rock.
void Burst(string name)
{
	float x = -14.0 + 28.0 * frnd();
	float z = -12.0 + 10.0 * frnd();
	SendMessage(&Particles, "lsffffffl", PS_CREATE, name, x, 0.0, z, 0.0, 0.0, 0.0, 0);
	Trace("PARTICLES: " + name + " at " + x + ", " + z);
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
	if (control == "Demo_Splash") { Burst("splash"); }
	if (control == "Demo_Explosion") { Burst("explosion"); }
	if (control == "Demo_Fire")
	{
		if (fireBurning == 1)
		{
			SendMessage(&Particles, "l", PS_CLEAR_CAPTURED);
			fireBurning = 0;
			Trace("PARTICLES: fire out");
		}
		else
		{
			LightFire();
		}
	}
	if (control == "Demo_Quit")
	{
		Trace("PARTICLES: quit");
		ExitProgram();
	}
}
