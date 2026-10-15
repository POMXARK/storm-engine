#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

struct SDL_Window;
union SDL_Event;
struct IDirect3DDevice9;
struct ImDrawList;

namespace storm::editor
{

enum class DebugFlag
{
    RenderWireframe,
    SoundDebug,
    LocationDebug,
    ExtendedLocationDebug,
    CannonDebug,
};

using EditorToolCallback = std::function<void(bool &active)>;
// the model that shows an entity which is no model itself (a character), 0 when the entity is not its kind
using ModelOfCallback = std::function<uint64_t(uint64_t entity)>;
// Play and Stop of the toolbar: true when the game starts to go, false when it stops back to the edit
using PlayCallback = std::function<void(bool playing)>;

// What the window Gizmo moves besides a model: a matrix a module hands out, a locator of a location.
// The functions find their object anew every time, so they stay safe when it is gone.
struct GizmoTarget
{
    // the entity it belongs to: the gizmo shows it while that entity is picked in Entities
    uint64_t owner = 0;
    std::string name;
    // the matrix (D3D layout, 16 floats), false when there is none any more
    std::function<bool(float *matrix)> get;
    std::function<void(const float *matrix)> set;
    // the line "Copy as script" gives for the matrix
    std::function<std::string(const float *matrix)> script;
};

// The free camera of the scene: while it is on, the renderer shows the frame from it instead of from the
// camera of the game (DX9RENDER::SetCamera); it starts where the camera of the game is
struct SceneCamera
{
    bool active = false;
    // false until the renderer has put it where the camera of the game is
    bool placed = false;
    float pos[3]{};
    // pitch, yaw, roll as CVECTOR angles of the renderer
    float ang[3]{};
    // metres a second
    float speed = 5.f;
};

// where the frame of the game is on the screen: the window Viewport of the workspace, else the whole screen
struct ViewportRect
{
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
};

class EngineEditor final
{
  public:
    // font: a TTF for the windows instead of the built-in one (which has Latin letters only), with
    // Cyrillic; empty or missing keeps the built-in font
    explicit EngineEditor(SDL_Window *window, IDirect3DDevice9 *device, const std::string &font = {},
                          float font_size = 0.f);
    ~EngineEditor();

    void StartFrame();
    void EndFrame();

    // the device is reset (the window resized, on Windows also lost): the objects of the default pool go
    // before and come back after
    void LostDevice();
    void RestoreDevice();

    void ProcessEvent(SDL_Event &event);

    bool IsFocused() const;

    bool IsDebugFlagEnabled(DebugFlag flag) const;

    // The factor of the time of the game the core puts on every frame: 0 while the editor holds the game
    // (Pause), 1 for the one frame of Step, else the speed picked in the toolbar
    float TimeScale();

    static void RegisterEditorTool(const std::string_view &title, EditorToolCallback callback);

    // A value a tool keeps between runs, in imgui.ini next to the open tools and the windows
    static std::string GetSetting(const std::string_view &key);
    static void SetSetting(const std::string_view &key, const std::string_view &value);

    // the target of the gizmo, nullptr when no module gave one
    static void SetGizmoTarget(GizmoTarget target);
    static const GizmoTarget *GetGizmoTarget();

    // A module tells how its entities are shown by a model (Character: its MODELR), so the windows that
    // work on a model (Animation) take the entity as well; ModelOf is 0 when no module knows the entity
    static void RegisterModelOf(ModelOfCallback callback);
    static uint64_t ModelOf(uint64_t entity);

    static SceneCamera &GetSceneCamera();

    // Edit or Play, as in the toolbar: in the edit the game stands still and the scene camera is on; Play lets
    // the game go, Stop brings the edit back. The tools that change the scene keep it at Play, give it back
    // at Stop.
    static bool IsPlaying();
    static void RegisterPlayCallback(PlayCallback callback);

    // What happened in the window Viewport this frame, for the tools that work in the scene: a file dropped
    // there from Assets (its path) and a left click, at a point of the screen; false when there was none.
    // Taken once.
    static bool TakeViewportDrop(std::string &asset, float &x, float &y);
    static bool TakeViewportClick(float &x, float &y);

    // The frame of the game on the screen and the draw list over it, for what is drawn in the scene (the gizmo)
    static ViewportRect GetViewportRect();
    static ImDrawList *GetViewportDrawList();

  private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace storm::editor
