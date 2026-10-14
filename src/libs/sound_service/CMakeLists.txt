if (STORM_SOUND_BACKEND STREQUAL "fmod")
    set(SOUND_BACKEND_DEPS fmod::fmod)
else()
    set(SOUND_BACKEND_DEPS miniaudio::miniaudio)
endif()

STORM_SETUP(
    TARGET_NAME sound_service
    TYPE storm_module
    DEPENDENCIES core ${SOUND_BACKEND_DEPS} renderer editor
)

if (STORM_SOUND_BACKEND STREQUAL "fmod")
    target_compile_definitions(sound_service PRIVATE STORM_SOUND_FMOD)
else()
    target_compile_definitions(sound_service PRIVATE STORM_SOUND_MINIAUDIO)
endif()
