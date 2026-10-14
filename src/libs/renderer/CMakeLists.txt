if (WIN32)
set(SYSTEM_DEPS "legacy_stdio_definitions" directx::directx)
else()
set(SYSTEM_DEPS "${NATIVE_D3D9_LIBS}")
endif()

STORM_SETUP(
    TARGET_NAME renderer
    TYPE storm_module
    DEPENDENCIES core config util ${SYSTEM_DEPS}
)
