# Scenes made in the editor: a .scene file (TOML) of models, markers and sound sources with their places, read by the script
# function LoadScene and by the window Scene of the editor
STORM_SETUP(
    TARGET_NAME scene
    TYPE storm_module
    DEPENDENCIES core model sound_service
    TEST_DEPENDENCIES Catch2::Catch2WithMain
)
