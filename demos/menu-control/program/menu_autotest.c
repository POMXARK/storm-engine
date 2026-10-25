// Menu control autotest: the main menu of the interface demo (its mainmenu.ini, the item list
// B_NEWGAME / B_LOAD / B_OPTIONS / B_QUIT) driven with nothing pressed at all. Every step is
// MSG_INTERFACE_MENU_ACTION 45062 of XINTERFACE, the message added for this demo: it names a command of
// xutil.cpp and a node, "" for the node the menu shows (xinterface.cpp:ProcessMessage), finds the
// node with CINODE::FindNode of inode.h and makes the same DoAction the keyboard branch of DoControl
// makes (xinterface.cpp:DoControl), so the command chain of a node, its "ievnt_command" report
// (inode.cpp:DoAction) and the node the chain returns work for a script exactly as they do for a
// player. Navigation is checked by reading the node the menu shows back with MSG_INTERFACE_GET_CURRENT_NODE
// 45028, the idiom of the game's interface/utilite.c, and comparing it with the node mainmenu.ini
// expects for that step; a selection is checked through the "activate" report of the node. The steps
// are chained with PostEvent with delays (no key presses are simulated anywhere), the result of every
// check lands in compile.log with a timestamp, and the run exits by itself.

#define MSG_INTERFACE_INIT 45010
#define MSG_INTERFACE_GET_CURRENT_NODE 45028
#define MSG_INTERFACE_MENU_ACTION 45062
#define INTERFACE_EXECUTE 4
#define INTERFACE_REALIZE 5
#define START_MS 600
#define STEP_MS 400

object GameInterface;

// the counters of the checks, the last report of a node command
int checksDone = 0;
int checksFailed = 0;
string lastCommand = "";
string lastCommandNode = "";
// the node the menu shows, filled in by ReadCurrentNode
string currentNodeName = "";

// the report of every command of the node the interface was told to act on: a step of the
// list reports its action (upstep, downstep), a selection reports activate
void Menu_Command()
{
    lastCommand = GetEventData();
    lastCommandNode = GetEventData();
    Trace("MENU: " + lastCommandNode + " " + lastCommand);
}

// the node the menu shows now, the idiom of the game's interface/utilite.c
void ReadCurrentNode()
{
    SendMessage(&GameInterface, "le", MSG_INTERFACE_GET_CURRENT_NODE, &currentNodeName);
}

// one action of the menu, of the node it shows ("") or of a node named by the script
void MenuAction(string action, string nodeName)
{
    SendMessage(&GameInterface, "lss", MSG_INTERFACE_MENU_ACTION, action, nodeName);
}

// the menu must show the expected node, the names come from mainmenu.ini
void CheckNode(string label, string expected)
{
    checksDone = checksDone + 1;
    if (currentNodeName == expected)
    {
        Trace("AUTOTEST: " + label + " shows " + currentNodeName);
        return;
    }
    checksFailed = checksFailed + 1;
    Trace("AUTOTEST: FAIL " + label + ", shows " + currentNodeName + ", want " + expected);
}

// a selection happened when the node reported the command "activate" under its own name
void CheckSelection(string label, string expected)
{
    checksDone = checksDone + 1;
    if (lastCommand == "activate" && lastCommandNode == expected)
    {
        Trace("AUTOTEST: " + label + ", node " + lastCommandNode + " reported activate");
        return;
    }
    ++checksFailed;
    Trace("AUTOTEST: FAIL " + label + ", report is " + lastCommandNode + " " + lastCommand + ", want " + expected + " activate");
}

void ReportResult()
{
    if (checksFailed == 0)
    {
        Trace("AUTOTEST: OK, " + checksDone + " checks of navigation and selection passed");
    }
    else
    {
        Trace("AUTOTEST: FAILED, " + checksFailed + " of " + checksDone + " checks did not pass");
    }
    DelEventHandler("ievnt_command", "Menu_Command");
    ExitProgram();
}

void Main()
{
    // no control is mapped on purpose: this run gets no keys and no clicks at all,
    // every action below comes from MSG_INTERFACE_MENU_ACTION
    CreateEntity(&GameInterface, "xinterface");
    LayerAddObject(INTERFACE_EXECUTE, &GameInterface, -100);
    LayerAddObject(INTERFACE_REALIZE, &GameInterface, -100);

    // the report of every command of a node, "ievnt_command" of inode.cpp:DoAction
    SetEventHandler("ievnt_command", "Menu_Command", 0);
    // the item list of the menu, the same ini the interface demo shows to a player
    SendMessage(&GameInterface, "ls", MSG_INTERFACE_INIT, "resource\ini\interfaces\mainmenu.ini");
    Trace("AUTOTEST: the menu is up, nothing pressed");
    // the steps of the autotest are the events chained by PostEvent, every one of them has
    // to be registered as a handler, the idiom of the island demo (island.c:Main)
    SetEventHandler("Menu_Start", "Menu_Start", 0);
    SetEventHandler("Menu_Down1", "Menu_Down1", 0);
    SetEventHandler("Menu_Down2", "Menu_Down2", 0);
    SetEventHandler("Menu_Down3", "Menu_Down3", 0);
    SetEventHandler("Menu_Up1", "Menu_Up1", 0);
    SetEventHandler("Menu_Up2", "Menu_Up2", 0);
    SetEventHandler("Menu_TopEdge", "Menu_TopEdge", 0);
    SetEventHandler("Menu_StayTop", "Menu_StayTop", 0);
    SetEventHandler("Menu_SelNamed", "Menu_SelNamed", 0);
    SetEventHandler("Menu_SelCurrent", "Menu_SelCurrent", 0);
    SetEventHandler("Menu_SelDone", "Menu_SelDone", 0);
    PostEvent("Menu_Start", START_MS);
}

// the menu opens on the item the ini names in "start" (mainmenu.ini: start = B_NEWGAME)
void Menu_Start()
{
    ReadCurrentNode();
    CheckNode("start", "B_NEWGAME");
    // "downstep,select:B_LOAD" of B_NEWGAME (mainmenu.ini: B_NEWGAME): one item down the list
    MenuAction("downstep", "");
    PostEvent("Menu_Down1", STEP_MS);
}

// every handler below checks the action of the step before it, the pending command of a node
// resolves in CINODE::FrameProcess of inode.cpp, so the node the menu shows is read back here
void Menu_Down1()
{
    ReadCurrentNode();
    CheckNode("down 1", "B_LOAD");
    // "downstep,select:B_OPTIONS" of B_LOAD (mainmenu.ini: B_LOAD)
    MenuAction("downstep", "");
    PostEvent("Menu_Down2", STEP_MS);
}

void Menu_Down2()
{
    ReadCurrentNode();
    CheckNode("down 2", "B_OPTIONS");
    // "downstep,select:B_QUIT" of B_OPTIONS (mainmenu.ini: B_OPTIONS)
    MenuAction("downstep", "");
    PostEvent("Menu_Down3", STEP_MS);
}

void Menu_Down3()
{
    ReadCurrentNode();
    CheckNode("down 3", "B_QUIT");
    // "upstep,select:B_OPTIONS" of B_QUIT (mainmenu.ini: B_QUIT): one item back up
    MenuAction("upstep", "");
    PostEvent("Menu_Up1", STEP_MS);
}

void Menu_Up1()
{
    ReadCurrentNode();
    CheckNode("up 1", "B_OPTIONS");
    // "upstep,select:B_LOAD" of B_OPTIONS (mainmenu.ini: B_OPTIONS)
    MenuAction("upstep", "");
    PostEvent("Menu_Up2", STEP_MS);
}

void Menu_Up2()
{
    ReadCurrentNode();
    CheckNode("up 2", "B_LOAD");
    // "upstep,select:B_NEWGAME" of B_LOAD (mainmenu.ini: B_LOAD): the top item of the list
    MenuAction("upstep", "");
    PostEvent("Menu_TopEdge", STEP_MS);
}

void Menu_TopEdge()
{
    ReadCurrentNode();
    CheckNode("up to the top", "B_NEWGAME");
    // B_NEWGAME declares no upstep (mainmenu.ini: B_NEWGAME), so this upstep of the current
    // node must find no command chain and the menu must keep its item
    MenuAction("upstep", "");
    PostEvent("Menu_StayTop", STEP_MS);
}

void Menu_StayTop()
{
    ReadCurrentNode();
    CheckNode("up out of the list", "B_NEWGAME");
    // the item named in the message is selected while the current one stays where it is:
    // the menu still shows B_NEWGAME while the script asks for B_OPTIONS, three items below
    MenuAction("activate", "B_OPTIONS");
    PostEvent("Menu_SelNamed", STEP_MS);
}

// an item named in the message is selected while the current one stays where it is: the menu
// still shows B_NEWGAME while the script asks for B_OPTIONS, three items below
void Menu_SelNamed()
{
    CheckSelection("activate of a named item", "B_OPTIONS");
    ReadCurrentNode();
    CheckNode("a named item does not move the menu", "B_NEWGAME");
    // "downstep,select:B_LOAD" of B_NEWGAME (mainmenu.ini: B_NEWGAME): the list still
    // moves from its top item, the named selection above did not move the menu
    MenuAction("downstep", "");
    PostEvent("Menu_SelCurrent", STEP_MS);
}

void Menu_SelCurrent()
{
    ReadCurrentNode();
    CheckNode("down from the top", "B_LOAD");
    // "activate" asked of the node the menu shows selects that node (mainmenu.ini: B_LOAD)
    MenuAction("activate", "");
    PostEvent("Menu_SelDone", STEP_MS);
}

void Menu_SelDone()
{
    CheckSelection("activate of the current item", "B_LOAD");
    ReportResult();
}
