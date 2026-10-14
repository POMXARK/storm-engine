// Dialog demo: a conversation with an old sailor on a pier. The "dialog" entity only draws the
// window: the speech of the current node (Dialog.Text) and the player's answers (Dialog.Links.*).
// When an answer is chosen it writes the answer's "go" into Dialog.CurrentNode and raises
// "DialogEvent"; the script fills the next node. The tree lives in Dialog_Node below and keeps its
// state in script variables: the player's name typed in an edit answer, coins, rum and a map.
// E talks to the sailor, Up/Down pick an answer, Enter, Space or the left button says it,
// PageUp/PageDown page a long speech, Escape leaves the conversation and then quits; the mouse and
// W/S move the camera.

#define EXECUTE 0
#define REALIZE 1
#define SEA_REFLECTION 15

#define MSG_MODEL_LOAD_GEO 20500
#define MSG_MODEL_LOAD_ANI 20501
#define MSG_MODEL_SET_POSITION 20506
#define MSG_MODEL_SET_TECHNIQUE 20510
#define MSG_MODEL_PLAY_ACTION 20601

// the deck of the pier, written by make_dialog.py
#define DECK_TOP 1.5

#define MAP_PRICE 30
#define RUM_PRICE 5

object Render;
object Scene;
object Camera;
object Weather;
object Sky;
object Sea;
object Pier;
object Sailor;
object Dialog;

int dialogOpen = 0;
int metBefore = 0;
string playerName = "";
int coins = 50;
int rumCups = 0;
int hasMap = 0;
int rumourIndex = 0;

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
	MapKey("Demo_Talk", 69);
	MapKey("Demo_Quit", 27);
	// polled by the dialog entity every frame while it is open
	MapKey("DlgUp", 38);
	MapKey("DlgDown", 40);
	MapKey("DlgAction", 32);
	MapKey("DlgAction1", 1);
	// Enter also confirms a typed answer, while Space types a space there
	MapKey("DlgAction2", 13);
	MapKey("DlgScrollUp", 33);
	MapKey("DlgScrollDown", 34);
	MapKey("DlgCancel", 27);

	Render.BackColor = argb(255, 0, 0, 0);

	// on the pier, looking along it at the sailor
	Camera.position.x = 0.4;
	Camera.position.y = DECK_TOP + 1.65;
	Camera.position.z = -1.2;
	CreateEntity(&Camera, "FREE_CAMERA");
	CreateEntity(&Scene, "Scene");
	LayerAddObject(EXECUTE, &Scene, 0);
	SendMessage(&Scene, "si", "SetActiveCamera", &Camera);

	CreateWorld();

	LoadModel(&Pier, "pier\pier");
	PlaceModel(&Pier, 0.0, 0.0, 0.0, 0.0);
	LayerAddObject(SEA_REFLECTION, &Pier, 10);

	LoadModel(&Sailor, "demo\sailor");
	SendMessage(&Sailor, "ls", MSG_MODEL_LOAD_ANI, "demo\sailor");
	SendMessage(&Sailor, "lls", MSG_MODEL_PLAY_ACTION, 0, "idle");
	// facing the camera
	PlaceModel(&Sailor, 0.0, DECK_TOP, 2.2, 3.1416);
	// the window shows "name lastname" of the entity it is given as the speaker
	Sailor.name = "Том";
	Sailor.lastname = "Бакстер";

	SetEventHandler("Control Activation", "Demo_Control", 0);
	SetEventHandler("DialogEvent", "Dialog_Event", 0);
	SetEventHandler("DialogCancel", "Dialog_Cancel", 0);
	SetEventHandler("EmergencyDialogExit", "Dialog_Cancel", 0);
	SetEventHandler("Demo_DialogClose", "Dialog_Close", 0);
	Trace("DIALOG: scene loaded");

	Dialog_Open();
}

// Sea, sky and weather as in the sea demo, with the day preset and a calm sea.
void CreateWorld()
{
	CreateEntity(&Weather, "weather");
	Weather.Fog.Enable = 1;
	Weather.Fog.Start = 0.0;
	Weather.Fog.Density = 0.002;
	Weather.Fog.Color = argb(0, 150, 185, 215);
	Weather.Sun.Color = argb(0, 230, 220, 205);
	Weather.Sun.Ambient = argb(0, 95, 100, 105);
	Weather.Sun.HeightAngle = 0.5;
	Weather.Sun.AzimuthAngle = 2.2;
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
	Sea.Sea2.Amp1 = 1.0;
	Sea.Sea2.AnimSpeed1 = 1.5;
	Sea.Sea2.Scale1 = 0.9;
	Sea.Sea2.MoveSpeed1 = "2.0, 0.0, 0.0";
	Sea.Sea2.Amp2 = 0.4;
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

// ---------------------------------------------------------------- the dialog entity

void Dialog_Open()
{
	if (dialogOpen == 1) { return; }
	// the entity pauses the game (time scale 0) and lets it run again over a second
	CreateEntity(&Dialog, "dialog");
	// message 1: the speaker, whose name/lastname the window shows, and his model
	SendMessage(&Dialog, "lii", 1, &Sailor, &Sailor);
	// drawn over everything else
	LayerAddObject(REALIZE, &Dialog, -256);
	dialogOpen = 1;
	if (metBefore == 1) { Dialog_Node("again"); }
	else { Dialog_Node("start"); }
	Trace("DIALOG: opened");
}

// Closing the dialog from its own event would delete the entity while it is drawing, so the
// handlers post an event that closes it on the next frame.
void Dialog_Close()
{
	if (dialogOpen == 0) { return; }
	DeleteClass(&Dialog);
	dialogOpen = 0;
	Trace("DIALOG: closed");
}

void Dialog_Leave()
{
	PostEvent("Demo_DialogClose", 1);
}

// an answer was chosen: its "go" is in Dialog.CurrentNode
void Dialog_Event()
{
	Dialog_Node(Dialog.CurrentNode);
}

// Escape, or an answer without "go" / with the same "go" as the answer before (the entity
// treats that as a broken tree)
void Dialog_Cancel()
{
	Trace("DIALOG: cancelled");
	Dialog_Leave();
}

void Say(string text)
{
	Dialog.Text = text;
	DeleteAttribute(&Dialog, "Links");
}

// The answers are shown in the order they are added; the text must be set before "go".
void Answer(string id, string text, string go)
{
	Dialog.Links.(id) = text;
	Dialog.Links.(id).go = go;
}

// ---------------------------------------------------------------- the tree

void Dialog_Node(string node)
{
	Trace("DIALOG: node " + node);
	if (node == "start")
	{
		Say("Эй, моряк! Что-то я тебя раньше в этой гавани не видал. Как тебя звать?");
		// an answer with "edit" is a text field: the typed text appears in Dialog.value
		Answer("l1", "Джек Хоук", "name");
		Dialog.Links.l1.edit = "string";
		Answer("l2", "Не твоё дело, старик.", "rude");
		return;
	}
	if (node == "name")
	{
		playerName = Dialog.value;
		if (playerName == "") { playerName = "Безымянный"; }
		metBefore = 1;
		Say("Рад знакомству, " + playerName + "! А я Том Бакстер, сорок лет хожу по морям, а теперь вот стерегу этот пирс. Чем могу помочь?");
		Menu();
		return;
	}
	if (node == "rude")
	{
		playerName = "Безымянный";
		metBefore = 1;
		Say("Хе! Гордый какой. Ну, дело твоё, Безымянный. Так чего тебе?");
		Menu();
		return;
	}
	if (node == "again")
	{
		Say("А, " + playerName + ", снова ты! Чего надобно?");
		Menu();
		return;
	}
	if (node == "menu")
	{
		Say("Ну, что ещё? В кошельке у тебя, гляжу, " + coins + " песо.");
		Menu();
		return;
	}
	if (node == "rumour" || node == "rumour_more")
	{
		Rumour(node);
		return;
	}
	if (node == "map")
	{
		if (hasMap == 1)
		{
			Say("Карта у тебя уже есть, другой не держу.");
			Answer("l1", "И то верно.", "menu");
			return;
		}
		Say("Карта острова Скелета, рисовал сам старый Флинт... ну, или его кок. Отдам за " + MAP_PRICE + " песо.");
		if (coins >= MAP_PRICE) { Answer("l1", "Беру!", "map_buy"); }
		Answer("l2", "Дороговато. Может, позже.", "menu");
		return;
	}
	if (node == "map_buy")
	{
		coins = coins - MAP_PRICE;
		hasMap = 1;
		Say("Держи. Говорят, клад зарыт у трёх пальм, что растут к северу от бухты. Только никому ни слова!");
		Answer("l1", "Буду нем как рыба.", "menu");
		return;
	}
	if (node == "rum" || node == "rum_more")
	{
		Rum(node);
		return;
	}
	if (node == "bye")
	{
		Say("Попутного ветра, " + playerName + "!");
		Answer("l1", "(уйти)", "exit");
		return;
	}
	if (node == "exit")
	{
		Dialog_Leave();
		return;
	}
	Trace("DIALOG: unknown node " + node);
	Dialog_Leave();
}

void Menu()
{
	Answer("l1", "Что нового в порту?", "rumour");
	if (hasMap == 0) { Answer("l2", "Говорят, у тебя есть карта острова?", "map"); }
	Answer("l3", "Налей-ка мне рому!", "rum");
	Answer("l4", "Прощай, Том.", "bye");
}

// Choosing the answer with the same "go" twice in a row closes the dialog as broken, so a node that
// repeats itself alternates two names.
void Rumour(string node)
{
	if (rumourIndex == 0)
	{
		// \n starts a new page: the answers show on the last one, PageDown or Enter turns it
		Say("Да всё по-старому. Вчера в бухту зашёл испанский галеон, весь в пробоинах. Говорят, у мыса его потрепали пираты\nА капитан его, дон Педро, до сих пор сидит в таверне и клянётся найти обидчиков, хоть на краю света.");
	}
	if (rumourIndex == 1)
	{
		Say("Губернатор опять поднял пошлины на ром. Контрабандисты потирают руки.");
	}
	if (rumourIndex == 2)
	{
		Say("Ночью над фортом видели огни. Солдаты говорят — призраки, а я думаю — контрабандисты сигналят.");
	}
	rumourIndex = rumourIndex + 1;
	if (rumourIndex > 2) { rumourIndex = 0; }
	if (node == "rumour") { Answer("l1", "А ещё что?", "rumour_more"); }
	else { Answer("l1", "А ещё что?", "rumour"); }
	Answer("l2", "Ладно, о другом.", "menu");
}

void Rum(string node)
{
	if (rumCups >= 3)
	{
		Say("Э, нет, " + playerName + ", тебе хватит. Ещё кружка — и ты пойдёшь на дно прямо с пирса.");
		Answer("l1", "Ик... Ладно.", "menu");
		return;
	}
	if (coins < RUM_PRICE)
	{
		Say("В долг не наливаю. Приходи с монетой.");
		Answer("l1", "Эх...", "menu");
		return;
	}
	coins = coins - RUM_PRICE;
	rumCups = rumCups + 1;
	if (rumCups == 1) { Say("Держи кружку. Ямайский, крепкий! С тебя " + RUM_PRICE + " песо."); }
	if (rumCups == 2) { Say("Вторая? Добрый ром, правда? Ещё " + RUM_PRICE + " песо."); }
	if (rumCups == 3) { Say("Ого, третья! Ну, твоё здоровье!"); }
	if (node == "rum") { Answer("l1", "Ещё кружку!", "rum_more"); }
	else { Answer("l1", "Ещё кружку!", "rum"); }
	Answer("l2", "Хватит пока.", "menu");
}

// ---------------------------------------------------------------- controls

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
	// while the dialog is open its own controls work and letters type into an edit answer
	if (dialogOpen == 1) { return; }
	if (control == "Demo_Talk") { Dialog_Open(); }
	if (control == "Demo_Quit")
	{
		Trace("DIALOG: quit");
		ExitProgram();
	}
}
