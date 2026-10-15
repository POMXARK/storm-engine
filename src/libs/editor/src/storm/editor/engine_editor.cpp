#include "storm/editor/engine_editor.hpp"

#include "core.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_internal.h>
#include <SDL2/SDL_events.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>

#include <imgui_impl_dx9.h>

namespace storm::editor
{

struct EditorToolConfig
{
    std::string title;
    EditorToolCallback callback;
    bool active = false;
};

class EngineEditor::Impl final {
public:
    // tools register from static constructors of other modules, so the lists are made on first use
    static std::vector<EditorToolConfig> &Tools()
    {
        static std::vector<EditorToolConfig> tools;
        return tools;
    }
    static std::map<std::string, std::string, std::less<>> &Settings()
    {
        static std::map<std::string, std::string, std::less<>> settings;
        return settings;
    }

    static std::optional<GizmoTarget> &Gizmo()
    {
        static std::optional<GizmoTarget> target;
        return target;
    }

    static std::vector<ModelOfCallback> &ModelsOf()
    {
        static std::vector<ModelOfCallback> callbacks;
        return callbacks;
    }

    static SceneCamera &Camera()
    {
        static SceneCamera camera;
        return camera;
    }

    static std::vector<PlayCallback> &PlayCallbacks()
    {
        static std::vector<PlayCallback> callbacks;
        return callbacks;
    }

    // a file dropped in Viewport or a click there, until a tool takes it; the frame after it is gone
    struct ViewportEvent
    {
        bool valid = false;
        std::string asset;
        float x = 0.f;
        float y = 0.f;
    };
    static ViewportEvent &Drop()
    {
        static ViewportEvent event;
        return event;
    }
    static ViewportEvent &Click()
    {
        static ViewportEvent event;
        return event;
    }

    static ViewportRect &Viewport()
    {
        static ViewportRect rect;
        return rect;
    }

    static ImDrawList *&ViewportDrawList()
    {
        static ImDrawList *list = nullptr;
        return list;
    }

    void Menus(bool workspace);
    void Toolbar();
    void Workspace();
    void ViewportWindow();
    void FlyCamera();
    void CopyFrame();
    void SetPlaying(bool playing);

    std::array<bool, 5> debugFlags_{};

    ImGuiContext *imgui_;
    IDirect3DDevice9 *device_ = nullptr;

    bool isFocused_ = true;

    // the frame of the game, copied from the back buffer for the window Viewport
    IDirect3DTexture9 *frame_ = nullptr;
    bool frameShown_ = false;

    // the edit (false) or the game going (Play); in the game it holds (Pause) and goes one frame (Step) at
    // the speed of the toolbar
    static bool &Playing()
    {
        static bool playing = true;
        return playing;
    }
    bool paused_ = false;
    bool step_ = false;
    float speed_ = 1.f;

    // the editor hidden, the game on the whole screen (F10)
    bool gameOnly_ = false;
    bool resetLayout_ = false;
    // the tab of the properties comes to the front once the windows are in the new layout
    bool focusInspector_ = false;
    bool viewportFocused_ = false;
    bool flying_ = false;
};


namespace
{

const char *kWorkspace = "Workspace";

bool IsWorkspace()
{
    return EngineEditor::GetSetting("Editor.Layout") != "overlay";
}

// The section [Storm][Editor] of imgui.ini: "Open=<tool>" for every open tool, "<key>=<value>" for
// the settings of the tools. ImGui reads it on the first frame, so the tools open where they were.
void AddSettingsHandler(std::vector<EditorToolConfig> &tools, std::map<std::string, std::string, std::less<>> &settings)
{
    static auto *tools_ = &tools;
    static auto *settings_ = &settings;
    ImGuiSettingsHandler handler;
    handler.TypeName = "Storm";
    handler.TypeHash = ImHashStr("Storm");
    handler.ReadOpenFn = [](ImGuiContext *, ImGuiSettingsHandler *, const char *) -> void * { return tools_; };
    handler.ReadLineFn = [](ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line) {
        const char *eq = std::strchr(line, '=');
        if (eq == nullptr)
        {
            return;
        }
        const std::string_view key(line, eq - line);
        const std::string_view value(eq + 1);
        if (key == "Open")
        {
            for (auto &config : *tools_)
            {
                config.active = config.active || config.title == value;
            }
        }
        else
        {
            (*settings_)[std::string(key)] = value;
        }
    };
    handler.WriteAllFn = [](ImGuiContext *, ImGuiSettingsHandler *, ImGuiTextBuffer *buf) {
        buf->append("[Storm][Editor]\n");
        for (const auto &config : *tools_)
        {
            if (config.active)
            {
                buf->appendf("Open=%s\n", config.title.c_str());
            }
        }
        for (const auto &[key, value] : *settings_)
        {
            buf->appendf("%s=%s\n", key.c_str(), value.c_str());
        }
        buf->append("\n");
    };
    ImGui::AddSettingsHandler(&handler);
}

// The first layout, or the one "Reset layout" gives: the entities on the left, the properties and the tools
// of the picked one on the right, the logs and the files below, the game in the middle
void BuildLayout(ImGuiID dock, ImVec2 size, const std::vector<EditorToolConfig> &tools)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, size);
    ImGuiID centre = dock;
    const ImGuiID left = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, 0.2f, nullptr, &centre);
    const ImGuiID right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.3f, nullptr, &centre);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.28f, nullptr, &centre);
    ImGui::DockBuilderDockWindow("Viewport", centre);
    ImGui::DockBuilderDockWindow("Entities", left);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Assets", bottom);
    // every other window of a tool, the game ones too, is a tab on the right, the properties of the
    // picked entity in front
    for (const auto &config : tools)
    {
        if (config.title != "Entities" && config.title != "Console" && config.title != "Assets")
        {
            ImGui::DockBuilderDockWindow(config.title.c_str(), right);
        }
    }
    ImGui::DockBuilderDockWindow("Entity Properties", right);
    ImGui::DockBuilderFinish(dock);
}

} // namespace

EngineEditor::EngineEditor(SDL_Window *window, IDirect3DDevice9 *device, const std::string &font, float font_size)
    : impl_(std::make_unique<Impl>())
{
    IMGUI_CHECKVERSION();
    impl_->imgui_ = ImGui::CreateContext();
    impl_->device_ = device;
    ImGuiIO &io = ImGui::GetIO();
    // the atlas is built on the first frame, so the font goes in before it
    if (!font.empty())
    {
        if (std::filesystem::exists(font))
        {
            io.Fonts->AddFontFromFileTTF(font.c_str(), font_size > 0.f ? font_size : 16.f, nullptr,
                                         io.Fonts->GetGlyphRangesCyrillic());
        }
        else
        {
            core.Trace("Editor: no font %s, the built-in one is used", font.c_str());
        }
    }
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    AddSettingsHandler(Impl::Tools(), Impl::Settings());

#ifdef _WIN32
    ImGui_ImplSDL2_InitForD3D(window);
#else
    // the SDL backend only feeds input here; it asserts on D3D off Windows, dxvk-native's d3d9 draws anyway
    ImGui_ImplSDL2_InitForOther(window);
#endif
    ImGui_ImplDX9_Init(device);
}

EngineEditor::~EngineEditor()
{
    if (impl_->frame_ != nullptr)
    {
        impl_->frame_->Release();
    }
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(impl_->imgui_);
}

void EngineEditor::Impl::Menus(bool workspace)
{
    if (ImGui::BeginMenu("Tools"))
    {
        for (auto &config : Tools())
        {
            if (ImGui::MenuItem(config.title.c_str(), NULL, &config.active))
            {
                ImGui::MarkIniSettingsDirty();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug"))
    {
        ImGui::MenuItem("Wireframe mode", NULL, &debugFlags_[static_cast<size_t>(DebugFlag::RenderWireframe)]);
        ImGui::MenuItem("Sound debug", NULL, &debugFlags_[static_cast<size_t>(DebugFlag::SoundDebug)]);
        ImGui::MenuItem("Location debug", NULL, &debugFlags_[static_cast<size_t>(DebugFlag::LocationDebug)]);
        ImGui::MenuItem("Extended location debug", NULL, &debugFlags_[static_cast<size_t>(DebugFlag::ExtendedLocationDebug)]);
        ImGui::MenuItem("Ship cannon debug", NULL, &debugFlags_[static_cast<size_t>(DebugFlag::CannonDebug)]);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        if (ImGui::MenuItem("Workspace", NULL, workspace))
        {
            SetSetting("Editor.Layout", workspace ? "overlay" : "workspace");
        }
        ImGui::SetItemTooltip("The game in the window Viewport among docked panels; off: the windows over the game");
        if (ImGui::MenuItem("Reset layout", NULL, false, workspace))
        {
            resetLayout_ = true;
        }
        ImGui::MenuItem("Game only", "F10", &gameOnly_);
        ImGui::EndMenu();
    }
}

// Play/Pause/Step, the speed of the time and the free camera: the toolbar of an editor, in the menu bar
void EngineEditor::Impl::Toolbar()
{
    ImGui::Separator();
    const bool playing = Playing();
    ImGui::PushStyleColor(ImGuiCol_Button, playing ? ImVec4(0.6f, 0.2f, 0.2f, 1.f) : ImVec4(0.2f, 0.45f, 0.2f, 1.f));
    if (ImGui::Button(playing ? "Stop" : "Play"))
    {
        SetPlaying(!playing);
    }
    ImGui::PopStyleColor();
    ImGui::SetItemTooltip(playing ? "Back to the edit (Ctrl+P): the game stands, the scene comes back as it was at Play"
                                  : "The game goes (Ctrl+P); the scene is kept and comes back at Stop");
    ImGui::SameLine();
    ImGui::BeginDisabled(!playing);
    if (ImGui::Button(paused_ ? "Resume" : "Pause"))
    {
        paused_ = !paused_;
    }
    ImGui::SetItemTooltip("Holds the time of the game: the scripts and the entities get no time, the frame is drawn");
    ImGui::SameLine();
    ImGui::BeginDisabled(!paused_);
    if (ImGui::Button("Step"))
    {
        step_ = true;
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("One frame of the game");
    ImGui::SameLine();
    constexpr float speeds[] = {0.1f, 0.25f, 0.5f, 1.f, 2.f, 4.f};
    auto label = [](float speed) {
        char text[16];
        std::snprintf(text, sizeof(text), "%gx", speed);
        return std::string(text);
    };
    ImGui::SetNextItemWidth(70.f);
    if (ImGui::BeginCombo("##speed", label(speed_).c_str()))
    {
        for (const float speed : speeds)
        {
            if (ImGui::Selectable(label(speed).c_str(), speed == speed_))
            {
                speed_ = speed;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("The speed of the time of the game");
    ImGui::SameLine();
    SceneCamera &camera = Camera();
    if (ImGui::Checkbox("Scene camera", &camera.active))
    {
        // it starts where the camera of the game is now
        camera.placed = false;
        SetSetting("Editor.SceneCamera", camera.active ? "1" : "0");
    }
    ImGui::SetItemTooltip("A free camera: the right mouse button in Viewport to look, WASD and Q/E to move, Shift faster, "
                          "the wheel the speed");
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f fps", ImGui::GetIO().Framerate);
    if (!Playing() || paused_)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1.f), Playing() ? "paused" : "edit");
    }
}

void EngineEditor::Impl::Workspace()
{
    if (ImGui::BeginMainMenuBar())
    {
        Menus(true);
        Toolbar();
        ImGui::EndMainMenuBar();
    }

    // a window under all the others over the work area, the dock space of the panels inside it
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                       ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin(kWorkspace, nullptr, flags);
    ImGui::PopStyleVar(3);
    const ImGuiID dock = ImGui::GetID("Dock");
    if (ImGui::DockBuilderGetNode(dock) == nullptr || resetLayout_)
    {
        BuildLayout(dock, viewport->WorkSize, Tools());
        resetLayout_ = false;
        focusInspector_ = true;
    }
    ImGui::DockSpace(dock);
    ImGui::End();

    ViewportWindow();
}

// The frame of the game in a window, its proportions kept. A click in it gives the input to the game
// (F1 takes it back); the right mouse button there flies the scene camera.
void EngineEditor::Impl::ViewportWindow()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    const bool open = ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar();
    // the frame is copied at the end of the frame when the window shows it
    frameShown_ = open;
    if (!open || frame_ == nullptr)
    {
        viewportFocused_ = false;
        ImGui::End();
        return;
    }
    D3DSURFACE_DESC desc;
    frame_->GetLevelDesc(0, &desc);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float scale = std::min(avail.x / desc.Width, avail.y / desc.Height);
    const ImVec2 size(desc.Width * scale, desc.Height * scale);
    const ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - size.x) / 2.f, cursor.y + (avail.y - size.y) / 2.f));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Image(frame_, size);
    // a file of Assets dropped on the frame, for the tools that put it into the scene
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("STORM_ASSET"))
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            Drop() = {true, std::string(static_cast<const char *>(payload->Data), payload->DataSize - 1), mouse.x, mouse.y};
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        Click() = {true, {}, mouse.x, mouse.y};
    }
    Viewport() = {origin.x, origin.y, size.x, size.y};
    ViewportDrawList() = ImGui::GetWindowDrawList();

    // in the edit the clicks pick in the scene, the game gets no input
    viewportFocused_ = ImGui::IsWindowFocused() && Playing();
    if (viewportFocused_ && ImGui::IsKeyPressed(ImGuiKey_F1, false))
    {
        ImGui::SetWindowFocus(nullptr);
        viewportFocused_ = false;
    }
    FlyCamera();

    // what holds the game now, in the corner of the frame
    std::string hint;
    auto add = [&hint](const char *text) { hint += hint.empty() ? text : std::string(" | ") + text; };
    if (!Playing())
    {
        add("Edit");
    }
    else if (paused_)
    {
        add("Paused");
    }
    if (Camera().active)
    {
        add("Scene camera");
    }
    if (viewportFocused_ && !flying_)
    {
        add("Input to the game, F1 back");
    }
    if (!hint.empty())
    {
        ImDrawList *draw = ImGui::GetWindowDrawList();
        const ImVec2 text = ImGui::CalcTextSize(hint.c_str());
        draw->AddRectFilled(ImVec2(origin.x + 4.f, origin.y + 4.f),
                            ImVec2(origin.x + text.x + 12.f, origin.y + text.y + 8.f), IM_COL32(0, 0, 0, 150), 3.f);
        draw->AddText(ImVec2(origin.x + 8.f, origin.y + 6.f), IM_COL32(255, 220, 120, 255), hint.c_str());
    }
    ImGui::End();
}

void EngineEditor::Impl::FlyCamera()
{
    SceneCamera &camera = Camera();
    const ImGuiIO &io = ImGui::GetIO();
    if (!camera.active || !camera.placed)
    {
        flying_ = false;
        return;
    }
    if (!flying_ && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        flying_ = true;
    }
    if (flying_ && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        flying_ = false;
    }
    if (!flying_)
    {
        return;
    }
    camera.ang[1] += io.MouseDelta.x * 0.004f;
    camera.ang[0] = std::clamp(camera.ang[0] + io.MouseDelta.y * 0.004f, -1.55f, 1.55f);
    if (io.MouseWheel != 0.f)
    {
        camera.speed = std::clamp(camera.speed * (io.MouseWheel > 0.f ? 1.25f : 0.8f), 0.5f, 200.f);
    }
    // the axes of the angles as the renderer builds them: x the pitch down, y the yaw
    const float cx = std::cos(camera.ang[0]), sx = std::sin(camera.ang[0]);
    const float cy = std::cos(camera.ang[1]), sy = std::sin(camera.ang[1]);
    const float forward[3] = {sy * cx, -sx, cy * cx};
    const float right[3] = {cy, 0.f, -sy};
    float move[3] = {};
    auto add = [&move](const float *axis, float k) {
        for (int i = 0; i < 3; ++i)
        {
            move[i] += axis[i] * k;
        }
    };
    add(forward, (ImGui::IsKeyDown(ImGuiKey_W) ? 1.f : 0.f) - (ImGui::IsKeyDown(ImGuiKey_S) ? 1.f : 0.f));
    add(right, (ImGui::IsKeyDown(ImGuiKey_D) ? 1.f : 0.f) - (ImGui::IsKeyDown(ImGuiKey_A) ? 1.f : 0.f));
    move[1] += (ImGui::IsKeyDown(ImGuiKey_E) ? 1.f : 0.f) - (ImGui::IsKeyDown(ImGuiKey_Q) ? 1.f : 0.f);
    // the real time, not the one of the game: the camera flies over a paused game too
    const float step = camera.speed * io.DeltaTime * (io.KeyShift ? 4.f : 1.f);
    for (int i = 0; i < 3; ++i)
    {
        camera.pos[i] += move[i] * step;
    }
}

void EngineEditor::Impl::SetPlaying(bool playing)
{
    Playing() = playing;
    paused_ = false;
    SetSetting("Editor.Mode", playing ? "play" : "edit");
    // the edit looks through the scene camera, the game through its own
    SceneCamera &camera = Camera();
    camera.active = !playing || GetSetting("Editor.SceneCamera") == "1";
    camera.placed = camera.placed && camera.active;
    for (const auto &callback : PlayCallbacks())
    {
        callback(playing);
    }
}

// the back buffer holds the frame of the game before the windows are drawn over it
void EngineEditor::Impl::CopyFrame()
{
    IDirect3DSurface9 *back = nullptr;
    if (FAILED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)))
    {
        return;
    }
    D3DSURFACE_DESC desc;
    back->GetDesc(&desc);
    if (frame_ != nullptr)
    {
        D3DSURFACE_DESC have;
        frame_->GetLevelDesc(0, &have);
        if (have.Width != desc.Width || have.Height != desc.Height || have.Format != desc.Format)
        {
            frame_->Release();
            frame_ = nullptr;
        }
    }
    if (frame_ == nullptr &&
        FAILED(device_->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT,
                                      &frame_, nullptr)))
    {
        core.Trace("Editor: no texture for the frame of the game, Viewport stays empty");
        frame_ = nullptr;
    }
    IDirect3DSurface9 *target = nullptr;
    if (frame_ != nullptr && SUCCEEDED(frame_->GetSurfaceLevel(0, &target)))
    {
        device_->StretchRect(back, nullptr, target, nullptr, D3DTEXF_NONE);
        target->Release();
    }
    back->Release();
}

void EngineEditor::StartFrame()
{
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    const ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_F10, false))
    {
        impl_->gameOnly_ = !impl_->gameOnly_;
    }
    // Ctrl+P is Play / Stop, as in Unity
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false) && !io.WantTextInput)
    {
        impl_->SetPlaying(!IsPlaying());
    }
    const bool workspace = IsWorkspace() && !impl_->gameOnly_;
    // the gizmo and the like draw over the whole screen unless the workspace shows the game in a window
    Impl::Viewport() = {0.f, 0.f, io.DisplaySize.x, io.DisplaySize.y};
    Impl::ViewportDrawList() = ImGui::GetBackgroundDrawList();
    impl_->frameShown_ = false;
    // what a tool has not taken in its frame is gone
    Impl::Drop().valid = false;
    Impl::Click().valid = false;
    impl_->flying_ = impl_->flying_ && workspace;
    // the scene camera of the last run is on again, from the camera of the game
    static bool restored = false;
    if (!restored && ImGui::GetFrameCount() > 1)
    {
        restored = true;
        Impl::Camera().active = GetSetting("Editor.SceneCamera") == "1";
        // the editor starts in the edit when it was left there
        if (GetSetting("Editor.Mode") == "edit")
        {
            Impl::Playing() = false;
            Impl::Camera().active = true;
            // the scripts have run till now: the tools give back what they changed, as at Stop
            for (const auto &callback : Impl::PlayCallbacks())
            {
                callback(false);
            }
        }
    }

    if (impl_->gameOnly_)
    {
        impl_->isFocused_ = false;
        core.GetWindow()->ShowCursor(false);
        return;
    }

    if (workspace)
    {
        impl_->Workspace();
    }
    else
    {
        if (ImGui::Begin("Developer Tools", NULL, ImGuiWindowFlags_MenuBar))
        {
            if (ImGui::BeginMenuBar())
            {
                impl_->Menus(false);
                ImGui::EndMenuBar();
            }
            impl_->Toolbar();
        }
        // End goes with every Begin, also when the window is collapsed and Begin is false
        ImGui::End();
    }

    for (auto &config : Impl::Tools())
    {
        if (config.active)
        {
            config.callback(config.active);
            if (!config.active)
            {
                // closed with the cross of its window
                ImGui::MarkIniSettingsDirty();
            }
        }
    }

    if (impl_->focusInspector_ && ImGui::FindWindowByName("Entity Properties") != nullptr)
    {
        ImGui::SetWindowFocus("Entity Properties");
        impl_->focusInspector_ = false;
    }

    // the game takes the keys and the mouse when its frame has the focus and the scene camera is not flown;
    // over the whole screen it takes them unless a window wants them
    impl_->isFocused_ = workspace ? !impl_->viewportFocused_ || impl_->flying_ : io.WantCaptureKeyboard;
    // the pointer is the editor's: the game in its frame and the flight of the scene camera take the mouse
    // hidden (on Linux relative: only its moves count)
    core.GetWindow()->ShowCursor(workspace ? !impl_->viewportFocused_ && !impl_->flying_ : impl_->isFocused_);
}

void EngineEditor::EndFrame()
{
    if (impl_->frameShown_)
    {
        impl_->CopyFrame();
    }
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void EngineEditor::ProcessEvent(SDL_Event &event)
{
    // the backend takes keys by the layout: with a Russian one S comes as a Cyrillic letter, unknown to ImGui,
    // and Ctrl+S, Ctrl+P, WASD do nothing. Letters and digits go by their place on the keyboard instead
    if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.keysym.sym >= 0x80)
    {
        const SDL_Scancode code = event.key.keysym.scancode;
        if (code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z)
        {
            event.key.keysym.sym = SDLK_a + (code - SDL_SCANCODE_A);
        }
        else if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9)
        {
            event.key.keysym.sym = SDLK_1 + (code - SDL_SCANCODE_1);
        }
        else if (code == SDL_SCANCODE_0)
        {
            event.key.keysym.sym = SDLK_0;
        }
    }
    ImGui_ImplSDL2_ProcessEvent(&event);
}

bool EngineEditor::IsFocused() const
{
    return impl_->isFocused_;
}

bool EngineEditor::IsDebugFlagEnabled(DebugFlag flag) const
{
    return impl_->debugFlags_[static_cast<size_t>(flag)];
}

float EngineEditor::TimeScale()
{
    if (!Impl::Playing())
    {
        return 0.f;
    }
    if (!impl_->paused_)
    {
        return impl_->speed_;
    }
    if (impl_->step_)
    {
        impl_->step_ = false;
        return 1.f;
    }
    return 0.f;
}

void EngineEditor::RegisterEditorTool(const std::string_view &title, EditorToolCallback callback)
{
    Impl::Tools().push_back({.title = std::string(title), .callback = callback});
}

std::string EngineEditor::GetSetting(const std::string_view &key)
{
    const auto it = Impl::Settings().find(key);
    return it == Impl::Settings().end() ? std::string() : it->second;
}

void EngineEditor::SetSetting(const std::string_view &key, const std::string_view &value)
{
    auto &stored = Impl::Settings()[std::string(key)];
    if (stored != value)
    {
        stored = value;
        if (ImGui::GetCurrentContext() != nullptr)
        {
            ImGui::MarkIniSettingsDirty();
        }
    }
}

void EngineEditor::SetGizmoTarget(GizmoTarget target)
{
    Impl::Gizmo() = std::move(target);
}

const GizmoTarget *EngineEditor::GetGizmoTarget()
{
    const auto &target = Impl::Gizmo();
    return target ? &*target : nullptr;
}

void EngineEditor::RegisterModelOf(ModelOfCallback callback)
{
    Impl::ModelsOf().push_back(std::move(callback));
}

uint64_t EngineEditor::ModelOf(uint64_t entity)
{
    for (const auto &callback : Impl::ModelsOf())
    {
        if (const uint64_t model = callback(entity))
        {
            return model;
        }
    }
    return 0;
}

SceneCamera &EngineEditor::GetSceneCamera()
{
    return Impl::Camera();
}

bool EngineEditor::IsPlaying()
{
    return Impl::Playing();
}

void EngineEditor::RegisterPlayCallback(PlayCallback callback)
{
    Impl::PlayCallbacks().push_back(std::move(callback));
}

bool EngineEditor::TakeViewportDrop(std::string &asset, float &x, float &y)
{
    Impl::ViewportEvent &drop = Impl::Drop();
    if (!drop.valid)
    {
        return false;
    }
    drop.valid = false;
    asset = drop.asset;
    x = drop.x;
    y = drop.y;
    return true;
}

bool EngineEditor::TakeViewportClick(float &x, float &y)
{
    Impl::ViewportEvent &click = Impl::Click();
    if (!click.valid)
    {
        return false;
    }
    click.valid = false;
    x = click.x;
    y = click.y;
    return true;
}

ViewportRect EngineEditor::GetViewportRect()
{
    return Impl::Viewport();
}

ImDrawList *EngineEditor::GetViewportDrawList()
{
    return Impl::ViewportDrawList();
}

} // namespace storm::editor
