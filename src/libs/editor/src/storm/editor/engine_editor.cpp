#include "storm/editor/engine_editor.hpp"

#include "core.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_internal.h>

#include <array>
#include <cstring>
#include <map>
#include <optional>

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

    std::array<bool, 5> debugFlags_{};

    ImGuiContext *imgui_;

    bool isFocused_ = true;
};


namespace
{

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

} // namespace

EngineEditor::EngineEditor(SDL_Window *window, IDirect3DDevice9 *device)
    : impl_(std::make_unique<Impl>())
{
    IMGUI_CHECKVERSION();
    impl_->imgui_ = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
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
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(impl_->imgui_);
}

void EngineEditor::StartFrame()
{
    const auto &io = ImGui::GetIO();
    impl_->isFocused_ = io.WantCaptureKeyboard;
    core.GetWindow()->ShowCursor(impl_->isFocused_);

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    if (ImGui::Begin("Developer Tools", NULL, ImGuiWindowFlags_MenuBar) )
    {
        if (ImGui::BeginMenuBar() )
        {
            if (ImGui::BeginMenu("Tools") )
            {
                for (auto &config : Impl::Tools())
                {
                    if (ImGui::MenuItem(config.title.c_str(), NULL, &config.active))
                    {
                        ImGui::MarkIniSettingsDirty();
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debug") )
            {
                ImGui::MenuItem("Wireframe mode", NULL, &impl_->debugFlags_[static_cast<size_t>(DebugFlag::RenderWireframe)]);
                ImGui::MenuItem("Sound debug", NULL, &impl_->debugFlags_[static_cast<size_t>(DebugFlag::SoundDebug)]);
                ImGui::MenuItem("Location debug", NULL, &impl_->debugFlags_[static_cast<size_t>(DebugFlag::LocationDebug)]);
                ImGui::MenuItem("Extended location debug", NULL, &impl_->debugFlags_[static_cast<size_t>(DebugFlag::ExtendedLocationDebug)]);
                ImGui::MenuItem("Ship cannon debug", NULL, &impl_->debugFlags_[static_cast<size_t>(DebugFlag::CannonDebug)]);
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
    }
    // End goes with every Begin, also when the window is collapsed and Begin is false
    ImGui::End();

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
}

void EngineEditor::EndFrame()
{
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void EngineEditor::ProcessEvent(SDL_Event &event)
{
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

} // namespace storm::editor
