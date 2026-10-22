#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#endif

#include <SDL2/SDL.h>
#include <mimalloc-new-delete.h>
#include <mimalloc.h>
#include <spdlog/spdlog.h>
#include <CLI/CLI.hpp>

#include "core_private.h"
#include "fs.h"
#include "input.hpp"
#include "lifecycle_diagnostics_service.hpp"
#include "logging.hpp"
#include "os_window.hpp"
#include "steam_api.hpp"
#include "storm/engine_settings.hpp"
#include "v_sound_service.h"
#include "watermark.hpp"

namespace
{

CorePrivate *core_private;

constexpr char defaultLoggerName[] = "system";
constexpr const char *DEFAULT_WINDOW_TITLE = "Storm Engine";
bool isRunning = false;
bool bActive = true;
bool bSoundInBackground = false;

storm::diag::LifecycleDiagnosticsService lifecycleDiagnostics;

void RunFrame()
{
    if (!core_private->Run())
    {
        isRunning = false;
    }

    lifecycleDiagnostics.notifyAfterRun();
}

#ifdef _WIN32
void RunFrameWithOverflowCheck()
{
    __try
    {
        RunFrame();
    }
    __except ([](unsigned code, struct _EXCEPTION_POINTERS *ep) {
        return code == EXCEPTION_STACK_OVERFLOW;
    }(GetExceptionCode(), GetExceptionInformation()))
    {
        _resetstkoflw();
        throw std::runtime_error("Stack overflow");
    }
}
#else
#define RunFrameWithOverflowCheck RunFrame
#endif

void mimalloc_fun(const char *msg, void *arg)
{
    static std::filesystem::path mimalloc_log_path;
    if (mimalloc_log_path.empty())
    {
        mimalloc_log_path =storm::GetEngineSettings().GetEnginePath(storm::EngineSettingsPathType::Logs) / "mimalloc.log";
        std::error_code ec;
        remove(mimalloc_log_path, ec);
    }

    FILE *mimalloc_log =
#ifdef _MSC_VER
        _wfopen(mimalloc_log_path.c_str(), L"a+b");
#else
        fopen(mimalloc_log_path.c_str(), "a+b");
#endif
    if (mimalloc_log != nullptr)
    {
        fputs(msg, mimalloc_log);
        fclose(mimalloc_log);
    }
}

#ifdef _WIN32
LONG WINAPI crash_dump_handler(_EXCEPTION_POINTERS *exceptionInfo)
{
    __try
    {
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

        wchar_t *lastSlash = nullptr;
        for (wchar_t *p = modulePath; *p; ++p)
            if (*p == L'\\') lastSlash = p;

        if (lastSlash != nullptr)
        {
            *lastSlash = L'\0';

            // Write raw stack backtrace to text file (works even in SEH context)
            wchar_t stackPath[MAX_PATH];
            wcscpy_s(stackPath, MAX_PATH, modulePath);
            wcscat_s(stackPath, MAX_PATH, L"\\crash_stack.txt");
            HANDLE hStack = CreateFileW(stackPath, GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hStack != INVALID_HANDLE_VALUE)
            {
                void *stack[64];
                unsigned int frames = CaptureStackBackTrace(0, 64, stack, nullptr);

                char buf[256];
                int len = _snprintf_s(buf, sizeof(buf),
                    "exception_addr=%p code=0x%08X\nmodule_base=%p\nframes=%u\n",
                    exceptionInfo->ExceptionRecord->ExceptionAddress,
                    exceptionInfo->ExceptionRecord->ExceptionCode,
                    GetModuleHandleW(nullptr), frames);
                DWORD written;
                WriteFile(hStack, buf, (DWORD)len, &written, nullptr);

                for (unsigned int i = 0; i < frames; ++i)
                {
                    len = _snprintf_s(buf, sizeof(buf), "frame[%u]=%p\n", i, stack[i]);
                    WriteFile(hStack, buf, (DWORD)len, &written, nullptr);
                }
                CloseHandle(hStack);
            }

            // Write minidump with thread info
            wchar_t dumpPath[MAX_PATH];
            wcscpy_s(dumpPath, MAX_PATH, modulePath);
            wcscat_s(dumpPath, MAX_PATH, L"\\crash.dmp");
            HANDLE file = CreateFileW(
                dumpPath, GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (file != INVALID_HANDLE_VALUE)
            {
                MINIDUMP_EXCEPTION_INFORMATION mdExc{};
                mdExc.ThreadId = GetCurrentThreadId();
                mdExc.ExceptionPointers = exceptionInfo;

                MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                                  (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithFullMemory | MiniDumpWithThreadInfo),
                                  &mdExc, nullptr, nullptr);
                CloseHandle(file);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

} // namespace

void HandleWindowEvent(const storm::OSWindow::Event &event)
{
    if (event == storm::OSWindow::Closed)
    {
        isRunning = false;
        if (core_private->initialized())
        {
            core_private->Event("DestroyWindow");
        }
    }
    else if (event == storm::OSWindow::FocusGained)
    {
        bActive = true;
        if (core_private->initialized())
        {
            core_private->AppState(bActive);
            if (const auto soundService = core.GetServiceX<VSoundService>();
                soundService && !bSoundInBackground)
            {
                soundService->SetActiveWithFade(true);
            }
        }
    }
    else if (event == storm::OSWindow::FocusLost)
    {
        bActive = false;
        if (core_private->initialized())
        {
            core_private->AppState(bActive);
            if (const auto soundService = core.GetServiceX<VSoundService>();
                soundService && !bSoundInBackground)
            {
                soundService->SetActiveWithFade(false);
            }
        }
    }
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(crash_dump_handler);
#endif

    CLI::App app("Storm Engine");

    bool enable_editor = false;
    app.add_flag("--editor", enable_editor, "Enable in-game editor");
    // the window of engine.ini, set from the command line by the launch scripts of a game
    std::string screen_size;
    app.add_option("--size", screen_size, "Window or screen size WxH, instead of screen_x and screen_y");
    bool cli_fullscreen = false;
    app.add_flag("--fullscreen", cli_fullscreen, "Full screen, instead of full_screen");
    bool cli_windowed = false;
    app.add_flag("--windowed", cli_windowed, "A window with borders, instead of full_screen and window_borders");
    bool cli_mouse_release = false;
    app.add_flag("--mouse-release", cli_mouse_release, "Ctrl+Alt lets the mouse go, a click takes it back");

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        return app.exit(e);
    }

    // Prevent multiple instances
#ifdef _WIN32 // CreateEventA
    if (!CreateEventA(nullptr, false, false, "Global\\FBBD2286-A9F1-4303-B60C-743C3D7AA7BE") ||
        GetLastError() == ERROR_ALREADY_EXISTS)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Another instance is already running!", nullptr);
        return EXIT_SUCCESS;
    }
#endif
    mi_register_output(mimalloc_fun, nullptr);
    mi_option_set(mi_option_show_errors, 1);
    mi_option_set(mi_option_show_stats, 0);
    mi_option_set(mi_option_eager_commit, 1);
    mi_option_set(mi_option_eager_region_commit, 1);
    mi_option_set(mi_option_large_os_pages, 1);
//    mi_option_set(mi_option_page_reset, 0);
//    mi_option_set(mi_option_segment_reset, 0);
    mi_option_set(mi_option_reserve_huge_os_pages, 1);
//    mi_option_set(mi_option_segment_cache, 16);
#ifdef _DEBUG
    mi_option_set(mi_option_verbose, 4);
#endif

    SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

    // Init diagnostics
    const auto lifecycleDiagnosticsGuard =
#ifdef STORM_ENABLE_CRASH_REPORTS
        lifecycleDiagnostics.initialize(true);
#else
        lifecycleDiagnostics.initialize(false);
#endif
    if (!lifecycleDiagnosticsGuard)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", "Unable to initialize lifecycle service!", nullptr);
    }
    else
    {
        lifecycleDiagnostics.setCrashInfoCollector([]() { core_private->collectCrashInfo(); });
    }

    // Read config
    auto ini = fio->OpenIniFile(fs::ENGINE_INI_FILE_NAME);
    if (!ini)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Engine config not found.", nullptr);
        return EXIT_FAILURE;
    }

    // Init stash
    create_directories(storm::GetEngineSettings().GetEnginePath(storm::EngineSettingsPathType::SaveData));

    // Init logging
    spdlog::set_default_logger(storm::logging::getOrCreateLogger(defaultLoggerName));
    spdlog::info("Logging system initialized. Running on {}", STORM_BUILD_WATERMARK_STRING);
    spdlog::info("mimalloc-redirect status: {}", mi_is_redirected());

    // Init core
    core_private = static_cast<CorePrivate *>(&core);
    core_private->EnableEditor(enable_editor);
    core_private->Init();

    uint32_t dwMaxFPS = static_cast<uint32_t>(ini->GetInt(nullptr, "max_fps", 0));
    auto bDebugWindow = ini->GetInt(nullptr, "DebugWindow", 0) == 1;
    auto bAcceleration = ini->GetInt(nullptr, "Acceleration", 0) == 1;
    if (ini->GetInt(nullptr, "logs", 1) == 0) // disable logging
    {
        spdlog::set_level(spdlog::level::off);
    }
    int width = ini->GetInt(nullptr, "screen_x", 1024);
    int height = ini->GetInt(nullptr, "screen_y", 768);
    int preferred_display = ini->GetInt(nullptr, "display", 0);
    bool fullscreen = ini->GetInt(nullptr, "full_screen", false);
    // the editor is a window among others: moved, resized and closed by its frame
    bool show_borders = ini->GetInt(nullptr, "window_borders", false) || enable_editor;
    if (int w = 0, h = 0; sscanf(screen_size.c_str(), "%dx%d", &w, &h) == 2 && w > 0 && h > 0)
    {
        width = w;
        height = h;
    }
    if (cli_fullscreen)
    {
        fullscreen = true;
    }
    if (cli_windowed)
    {
        fullscreen = false;
        show_borders = true;
    }
    // Linux keeps the mouse relative, held in the window (sdl_input.cpp); mouse_release = 1 lets it go by
    // Ctrl+Alt, the input then ignores its moves, and a click in the window takes it back
    const bool mouse_release = ini->GetInt(nullptr, "mouse_release", 0) != 0 || cli_mouse_release;
    if (mouse_release)
    {
        SDL_SetHint(STORM_HINT_MOUSE_RELEASE, "1");
    }
    bool run_in_background = ini->GetInt(nullptr, "run_in_background", false);
    if (run_in_background) 
    {
        bSoundInBackground = ini->GetInt(nullptr, "sound_in_background", true);
    }
    else 
    {
        bSoundInBackground = false;
    }
    bool bSteam = ini->GetInt(nullptr, "Steam", 1) != 0;
    
    // initialize SteamApi through evaluating its singleton
    try
    {
        steamapi::SteamApi::getInstance(!bSteam);
    }
    catch (const std::exception &e)
    {
        spdlog::critical(e.what());
        return EXIT_FAILURE;
    }

    std::shared_ptr<storm::OSWindow> window =
        storm::OSWindow::Create(width, height, preferred_display, fullscreen, show_borders);

    char title[256]{};
    ini->ReadString(nullptr, "title", title, sizeof(title), DEFAULT_WINDOW_TITLE);
    window->SetTitle(title);

    window->Subscribe(HandleWindowEvent);
    window->Show();
    core_private->SetWindow(window);

    // Init core
    core_private->InitBase();

    // Message loop
    auto dwOldTime = SDL_GetTicks();

    isRunning = true;
    while (isRunning)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event) )
        {
#ifndef _WIN32
            if (mouse_release)
            {
                const bool ctrl_alt = (SDL_GetModState() & KMOD_CTRL) && (SDL_GetModState() & KMOD_ALT);
                if (event.type == SDL_KEYDOWN && ctrl_alt && SDL_GetRelativeMouseMode())
                {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN && !SDL_GetRelativeMouseMode())
                {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
            }
#endif
            if (auto *editor = core.GetEditor(); editor != nullptr) {
                editor->ProcessEvent(event);
            }
        }

        if (bActive || run_in_background)
        {
            if (dwMaxFPS)
            {
                const auto dwMS = 1000u / dwMaxFPS;
                const auto dwNewTime = SDL_GetTicks();
                if (dwNewTime - dwOldTime < dwMS)
                    continue;
                dwOldTime = dwNewTime;
            }

            RunFrameWithOverflowCheck();
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (core.Controls && core.Controls->GetDebugAsyncKeyState(VK_F1) && core.Controls->GetDebugAsyncKeyState(VK_SHIFT))
        {
            mi_stats_print_out(mimalloc_fun, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Release
    core_private->Event("ExitApplication");
    core_private->CleanUp();
    core_private->ReleaseBase();
#ifdef _WIN32 // FIX_LINUX Cursor
    ClipCursor(nullptr);
#elif _DEBUG
    mi_option_set(mi_option_verbose, 0); // disable statistics writing in Linux
#endif
    SDL_Quit();

    return EXIT_SUCCESS;
}
