// Smoke test: creates the Sound entity (initializes the sound service and the DX9 renderer),
// plays a tone and exits after a fixed number of frames. In the middle of the run the screen is
// requested to another size: the window and the back buffer must take it without a restart.

#define MSG_SOUND_PLAY 77003
#define SOUND_WAV_STEREO 4
#define VOLUME_FX 1
#define SMOKE_FRAMES 180
#define RESIZE_FRAME 60
#define CHECK_FRAME 70
#define RESIZE2_FRAME 110
#define CHECK2_FRAME 120

object Sound;
object Render;
int smokeFrames = 0;

void Main()
{
	Trace("SMOKE: Main started");

	CreateEntity(&Sound, "Sound");
		int soundId = SendMessage(Sound, "lsll", MSG_SOUND_PLAY, "smoke_tone.wav", SOUND_WAV_STEREO, VOLUME_FX);
		Trace("SMOKE: sound id " + soundId);

		SetEventHandler("frame", "Smoke_Frame", 1);
}

void Smoke_Frame()
{
		smokeFrames++;
		if (smokeFrames == RESIZE_FRAME)
		{
			Trace("SMOKE: a resize to 1280x720 is requested in the middle of a run");
			Render.SetScreen.x = 1280;
			Render.SetScreen.y = 720;
			Render.SetScreen.full_screen = 0;
		}
		if (smokeFrames == RESIZE2_FRAME)
		{
			Trace("SMOKE: the screen is asked again, to a smaller 960x540");
			// two changes in one run: the device must follow the resized window every time
			Render.SetScreen.x = 960;
			Render.SetScreen.y = 540;
			Render.SetScreen.full_screen = 0;
		}
		if (smokeFrames == CHECK_FRAME)
		{
			Trace("SMOKE: the frames after the resize are rendered");
		}
		if (smokeFrames == CHECK2_FRAME)
		{
			Trace("SMOKE: the frames after the second resize are rendered");
		}
		if (smokeFrames == SMOKE_FRAMES)
		{
			Trace("SMOKE: OK, " + smokeFrames + " frames rendered");
			DelEventHandler("frame", "Smoke_Frame");
			ExitProgram();
		}
}
