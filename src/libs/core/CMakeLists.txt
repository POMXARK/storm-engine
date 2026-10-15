STORM_SETUP(
    TARGET_NAME core
    TYPE library
    DEPENDENCIES diagnostics editor math shared_headers sound steam_api FastFloat::fast_float ${SDL2_LIBRARIES} window tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json
)

# the tool window of tm_list.cpp calls InitCommonControlsEx: whoever links core needs comctl32, scene-test too
if (WIN32)
    target_link_libraries(core PUBLIC comctl32)
endif()
