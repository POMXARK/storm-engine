// Smoke test: creates the Sound entity (initializes the sound service and the DX9 renderer),
// plays a tone and exits after a fixed number of frames.

#define MSG_SOUND_PLAY 77003
#define SOUND_WAV_STEREO 4
#define VOLUME_FX 1
#define SMOKE_FRAMES 180

object Sound;
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
	if (smokeFrames == SMOKE_FRAMES)
	{
		Trace("SMOKE: OK, " + smokeFrames + " frames rendered");
		DelEventHandler("frame", "Smoke_Frame");
		ExitProgram();
	}
}
