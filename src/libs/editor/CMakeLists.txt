if (NOT WIN32)
    set(SYSTEM_DEPS "${NATIVE_D3D9_LIBS}")
endif()

STORM_SETUP(
        TARGET_NAME editor
        TYPE storm_module
        DEPENDENCIES core
)

### ImGui
if (ImGui_FOUND)
    set(IMGUI_BINGINDS_DIR "${CMAKE_BINARY_DIR}/imgui")

    add_library(imgui_backend STATIC
            "${IMGUI_BINGINDS_DIR}/imgui_impl_sdl2.cpp"
            "${IMGUI_BINGINDS_DIR}/imgui_stdlib.cpp"
            "${IMGUI_BINGINDS_DIR}/imgui_impl_dx9.cpp"
    )

    target_include_directories(imgui_backend PUBLIC ${IMGUI_BINGINDS_DIR})
    if (NOT WIN32)
        # the dx9 binding clears its structs with the Win32 ZeroMemory, which dxvk-native does not define
        target_compile_options(imgui_backend PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_dx9_posix.h")
    endif()
    target_link_libraries(imgui_backend PUBLIC imgui::imgui ${SDL2_LIBRARIES} ${SYSTEM_DEPS})

    target_link_libraries(editor PUBLIC imgui_backend)
endif()
###
