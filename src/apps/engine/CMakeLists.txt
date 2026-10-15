# this is something I really don't like but it seems there's not better way to ensure mimalloc is linked before c-runtime
#include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
#if (WIN32)
#  set(MIMALLOC_DEP "$<IF:$<CONFIG:Release>,${CONAN_LIB_DIRS_MIMALLOC_RELEASE}/mimalloc.lib,${CONAN_LIB_DIRS_MIMALLOC_DEBUG}/mimalloc-debug.lib>")
#else()
#  set(MIMALLOC_DEP "$<IF:$<CONFIG:Release>,${CONAN_LIB_DIRS_MIMALLOC_RELEASE}/libmimalloc.so,${CONAN_LIB_DIRS_MIMALLOC_DEBUG}/libmimalloc-debug.so>")
#endif()

if (WIN32)
set(SYSTEM_DEPS
    "comctl32"
    "dbghelp"
    "winhttp"
)
else()
set(SYSTEM_DEPS "ffi")
endif()

STORM_SETUP(
    TARGET_NAME engine
    TYPE executable
    DEPENDENCIES

    ${MIMALLOC_DEP}

    # internal
    core
    diagnostics
    util
    window

    # external
    mimalloc
    sentry::sentry
    ${SDL2_LIBRARIES}
    ZLIB::ZLIB
    CLI11::CLI11

    # system
    ${SYSTEM_DEPS}
)

add_dependencies(engine engine_techniques)

# the glTF converter goes next to the engine: the window Assets of the editor runs it (button Import)
add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:engine>/tools/gltf2gm
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/tools/gltf2gm/gltf2gm.py
            ${PROJECT_SOURCE_DIR}/demos/common/storm_formats.py
            $<TARGET_FILE_DIR:engine>/tools/gltf2gm/
)
