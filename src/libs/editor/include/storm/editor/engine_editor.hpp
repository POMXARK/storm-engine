#pragma once

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

  private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace storm::editor
