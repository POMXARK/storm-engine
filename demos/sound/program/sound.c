// Sound demo: music, 3D sounds and the ambient scheme of the sound service, around the brig of the
// ship demo. A buoy with a bell rocks at sea, its bell is a looped 3D sound: it gets quieter as the
// ship sails away and moves between the ears as the camera turns. Space fires a cannon, a 3D sound
// at the ship played through an alias that picks one of three shots. The ambient scheme loops the
// surf and lets gulls cry at random. M switches the music between a calm and a battle arrangement
// with a crossfade, Up/Down set the music volume, G turns the ambient scheme off and on.
// A/D turn, W/S set and furl the sails, the mouse turns the camera, Escape quits.

#define EXECUTE 0
#define REALIZE 1
// shared/layers.h: mirrored into the sea reflection (sky in the first, ships in the second)
#define SEA_REFLECTION 15
#define SEA_REFLECTION2 16
// shared/layers.h: what the cannon balls and cameras trace against
#define SAILS_TRACE 19

// shared/messages.h and shared/sea_ai/script_defines.h
#define MSG_MODEL_LOAD_GEO 20500
#define MSG_MODEL_SET_POSITION 20506
#define MSG_MODEL_SET_TECHNIQUE 20510
#define MSG_SHIP_CREATE 50300
#define AI_MESSAGE_SET_LAYERS 51015
#define AI_CAMERAS_SET_CAMERA 55001

// shared/messages.h, the messages of the sound entity
// "sllllll[fffff]": file or alias, type, volume type, only cache, looped, cached, fade in ms, then
// for a 3D sound its position and min/max distance; returns the id of the sound
#define MSG_SOUND_PLAY 77003
// "ll": id (0 = all), fade out ms (after the message code)
#define MSG_SOUND_STOP 77004
// "llfff": id, SOUND_PARAM_POSITION, x, y, z
#define MSG_SOUND_SET_3D_PARAM 77007
// "fff": master volumes of effects, music and speech
#define MSG_SOUND_SET_MASTER_VOLUME 77021
#define MSG_SOUND_SCHEME_RESET 77013
#define MSG_SOUND_SCHEME_SET 77014
// shared/sound.h
#define SOUND_PARAM_POSITION 3

// sound_service/include/sound_defines.h: eSoundType and eVolumeType
#define PCM_3D 1
#define MP3_STEREO 2
#define PCM_STEREO 4
#define VOLUME_FX 1
#define VOLUME_MUSIC 2

// shared/battle_interface/log_msg.h
#define LOG_ADD_STRING 45020
#define LOG_AND_ACTIONS_INIT 45022

// where the buoy floats, and how far its bell is heard
#define BUOY_X 0.0
#define BUOY_Z 70.0
#define BELL_MIN_DISTANCE 15.0
#define BELL_MAX_DISTANCE 260.0

// priorities of the ship in the execute and realize layers; the ship entity reads these globals
// by name and its sails, blots and camera are placed right after it
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
object Buoy;
object BaseShip;
object Player;
object SailTextures;
// the script side of the sound service: every sound is played through messages to it
object Sound;

int bellSound = 0;
int battleMusic = 0;
float musicVolume = 0.6;
int ambient = 1;
// the surf is looped by the script itself, so it has an id to stop it by
int wavesSound = 0;
float buoyTime = 0.0;
// the distance to the bell is logged when it changes by a step, so the log follows the ship
int loggedStep = -1;

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
	MapKey("ShipCamera_Forward", 81);
	MapKey("ShipCamera_Backward", 69);
	MapKey("Demo_Fire", 32);
	MapKey("Demo_Music", 77);
	MapKey("Demo_Ambient", 71);
	MapKey("Demo_MusicUp", 38);
	MapKey("Demo_MusicDown", 40);
	MapKey("Demo_Quit", 27);

	Weather.Wind.Angle = 0.0;
	Weather.Wind.Speed = 10.0;
	// the ship starts 60 m off the buoy and passes it on the right
	Player.ship.pos.x = 22.0;
	Player.ship.pos.z = 10.0;
	Player.ship.ang.y = 0.0;
	Player.Ship.Speed.z = 1.0;

	Render.BackColor = argb(255, 0, 0, 0);
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	CreateWorld();
	CreateBuoy();
	CreateShip();
	CreateCamera();
	CreateLog();
	CreateSound();

	SetEventHandler("frame", "Demo_Frame", 0);
	SetEventHandler("Control Activation", "Demo_Control", 0);
	Log("Space fire, M music, Up/Down volume, G ambient");
	Trace("SOUND: scene loaded");
}

// The sound entity is the script's handle on the sound service. The listener needs no code: the
// service takes it from the view matrix of the renderer every frame, so it is wherever the camera is.
void CreateSound()
{
	CreateEntity(&Sound, "Sound");
	SendMessage(&Sound, "lfff", MSG_SOUND_SET_MASTER_VOLUME, 1.0, musicVolume, 1.0);

	// a looped 3D sound: full volume within the min distance, fading linearly to silence at the max
	bellSound = SendMessage(&Sound, "lsllllllfffff", MSG_SOUND_PLAY, "demo\bell.wav", PCM_3D, VOLUME_FX,
		false, true, false, 0, BUOY_X, 2.0, BUOY_Z, BELL_MIN_DISTANCE, BELL_MAX_DISTANCE);

	StartAmbient();
	PlayMusic();
	Trace("SOUND: bell id " + bellSound);
}

// Music is streamed from disk and has two slots: a new track fades in while the one playing fades
// out over the same time. The service also remembers where a track stopped and resumes it from there.
void PlayMusic()
{
	string track = "music\calm.wav";
	if (battleMusic) { track = "music\battle.wav"; }
	// file, type, volume type, only cache, looped, cached, fade in ms, pause between loops, volume
	SendMessage(&Sound, "lslllllllf", MSG_SOUND_PLAY, track, MP3_STEREO, VOLUME_MUSIC, false, true, false, 3000, 0, 1.0);
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

	// a calmer swell than in the ship demo, so the buoy, which only bobs, stays above the water
	CreateEntity(&Sea, "sea");
	LayerAddObject(EXECUTE, &Sea, 4);
	LayerAddObject(REALIZE, &Sea, 65530);
	Sea.Sea2.WaterColor = argb(0, 30, 90, 115);
	Sea.Sea2.SkyColor = argb(0, 205, 255, 255);
	Sea.Sea2.Amp1 = 1.2;
	Sea.Sea2.AnimSpeed1 = 1.5;
	Sea.Sea2.Scale1 = 0.6;
	Sea.Sea2.MoveSpeed1 = "2.0, 0.0, 0.0";
	Sea.Sea2.Amp2 = 0.5;
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

void CreateBuoy()
{
	CreateEntity(&Buoy, "modelr");
	SendMessage(&Buoy, "ls", MSG_MODEL_LOAD_GEO, "demo\buoy");
	SendMessage(&Buoy, "ls", MSG_MODEL_SET_TECHNIQUE, "DLightModel");
	LayerAddObject(EXECUTE, &Buoy, 10);
	LayerAddObject(REALIZE, &Buoy, 10);
	LayerAddObject(SEA_REFLECTION2, &Buoy, 10);
	PlaceBuoy();
}

// the buoy bobs and rocks on the swell; the bell sound stays where the buoy is
void PlaceBuoy()
{
	float y = 0.4 * sin(buoyTime * 1.3) - 0.3;
	float a = 0.08 * sin(buoyTime * 1.1);
	float c = cos(a);
	float s = sin(a);
	SendMessage(&Buoy, "lffffffffffff", MSG_MODEL_SET_POSITION, BUOY_X, y, BUOY_Z, 1.0, 0.0, 0.0, 0.0, c, s, 0.0, 0.0 - s, c);
}

void CreateShip()
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

	Player.id = "Player";
	Player.index = 0;
	Player.nation = 0;
	Player.MainCharacter = 1;

	SetEventHandler("SeaGetLayers", "Demo_ShipLayers", 0);
	SetEventHandler("Shp_UpdParams", "Demo_ShipParameters", 0);
	SetEventHandler("GetSailTextureData", "Demo_SailTextures", 0);
	CreateEntity(&Player, "ship");
	SendMessage(&Player, "laa", MSG_SHIP_CREATE, &Player, &BaseShip);
	SendMessage(&Player, "ls", 20510, "DLightModel");
	Demo_UpdateSpeeds(1.0);
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

void Demo_Frame()
{
	buoyTime = buoyTime + GetDeltaTime() * 0.001;
	PlaceBuoy();

	// the bell is how loud the service makes it: 1 within the min distance, 0 past the max
	float dx = stf(Player.ship.pos.x) - BUOY_X;
	float dz = stf(Player.ship.pos.z) - BUOY_Z;
	float distance = sqrt(dx * dx + dz * dz);
	int step = makeint(distance / 20.0);
	if (step != loggedStep)
	{
		loggedStep = step;
		float volume = 1.0 - (distance - BELL_MIN_DISTANCE) / (BELL_MAX_DISTANCE - BELL_MIN_DISTANCE);
		if (volume > 1.0) { volume = 1.0; }
		if (volume < 0.0) { volume = 0.0; }
		Log("Bell " + makeint(distance) + " m away, volume " + makeint(volume * 100.0) + "%");
		Trace("SOUND: ship " + makeint(distance) + " m from the bell, volume " + makeint(volume * 100.0));
	}
}

void Demo_Fire()
{
	// the shot sounds from the ship; the alias in resource\ini\aliases\demo.ini picks the file and
	// the distances, so only the position is given here
	float x = stf(Player.ship.pos.x);
	float z = stf(Player.ship.pos.z);
	int id = SendMessage(&Sound, "lsllllllfff", MSG_SOUND_PLAY, "cannon", PCM_3D, VOLUME_FX, false, false, false, 0,
		x, 3.0, z);
	Log("Fire!");
	Trace("SOUND: cannon id " + id);
}

void Demo_SwitchMusic()
{
	battleMusic = 1 - battleMusic;
	PlayMusic();
	if (battleMusic) { Log("Music: battle"); } else { Log("Music: calm"); }
	Trace("SOUND: battle music " + battleMusic);
}

void Demo_MusicVolume(float delta)
{
	musicVolume = musicVolume + delta;
	if (musicVolume < 0.0) { musicVolume = 0.0; }
	if (musicVolume > 1.0) { musicVolume = 1.0; }
	SendMessage(&Sound, "lfff", MSG_SOUND_SET_MASTER_VOLUME, 1.0, musicVolume, 1.0);
	Log("Music volume " + makeint(musicVolume * 100.0 + 0.5) + "%");
}

// The ambient of a place is a section of resource\ini\sound_scheme.ini: the gulls cry at random
// intervals. The surf is looped here rather than by a ch_loop line of the scheme: the service does
// not keep the id of a looped scheme channel, and resetting the scheme would leave it playing.
void StartAmbient()
{
	SendMessage(&Sound, "ls", MSG_SOUND_SCHEME_SET, "sea_day");
	wavesSound = SendMessage(&Sound, "lsllllll", MSG_SOUND_PLAY, "demo\waves.wav", PCM_STEREO, VOLUME_FX,
		false, true, false, 2000);
}

void Demo_ToggleAmbient()
{
	ambient = 1 - ambient;
	if (ambient)
	{
		StartAmbient();
		Log("Ambient on");
	}
	else
	{
		// a reset scheme stops starting its sounds, a gull already crying finishes
		SendMessage(&Sound, "l", MSG_SOUND_SCHEME_RESET);
		SendMessage(&Sound, "lll", MSG_SOUND_STOP, wavesSound, 2000);
		Log("Ambient off");
	}
	Trace("SOUND: ambient " + ambient);
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
	Demo_UpdateSpeeds(sails);
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
	if (control == "Demo_Fire") { Demo_Fire(); }
	if (control == "Demo_Music") { Demo_SwitchMusic(); }
	if (control == "Demo_MusicUp") { Demo_MusicVolume(0.2); }
	if (control == "Demo_MusicDown") { Demo_MusicVolume(-0.2); }
	if (control == "Demo_Ambient") { Demo_ToggleAmbient(); }
	if (control == "Demo_Quit")
	{
		Trace("SOUND: quit");
		ExitProgram();
	}
}
