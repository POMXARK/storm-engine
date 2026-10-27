if (WIN32)
set(SYSTEM_DEPS
    "ddraw"
    "amstrmid"
)
else()
# the video of Linux is decoded by FFmpeg (aviplayer_ffmpeg.cpp)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswresample)
set(SYSTEM_DEPS PkgConfig::FFMPEG)
endif()

STORM_SETUP(
    TARGET_NAME xinterface
    TYPE storm_module
    DEPENDENCIES animation core geometry model renderer sound_service util ${SYSTEM_DEPS}
    TEST_DEPENDENCIES Catch2::Catch2WithMain
)