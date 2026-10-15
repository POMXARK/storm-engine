#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

struct SDL_Window;
union SDL_Event;
struct IDirect3DDevice9;

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

class EngineEditor final
{
  public:
    explicit EngineEditor(SDL_Window *window, IDirect3DDevice9 *device);
    ~EngineEditor();

    void StartFrame();
    void EndFrame();

    void ProcessEvent(SDL_Event &event);

    bool IsFocused() const;

    bool IsDebugFlagEnabled(DebugFlag flag) const;

    static void RegisterEditorTool(const std::string_view &title, EditorToolCallback callback);

    // A value a tool keeps between runs, in imgui.ini next to the open tools and the windows
    static std::string GetSetting(const std::string_view &key);
    static void SetSetting(const std::string_view &key, const std::string_view &value);

    // the target of the gizmo, nullptr when no module gave one
    static void SetGizmoTarget(GizmoTarget target);
    static const GizmoTarget *GetGizmoTarget();

  private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace storm::editor
