cmake_minimum_required(VERSION 3.24)
# Windows: link the C++ runtime into the plug-in itself, so a DAW's plug-in scanner never trips over missing runtime DLLs
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(SongSavant VERSION 0.6.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(SS_BUILD_TESTS "Build the headless test + screenshot runner" ON)

# ------------------------------------------------------------------------------------------------
# JUCE (fetched automatically; set FETCHCONTENT_SOURCE_DIR_JUCE to a local copy to skip the download)
# ------------------------------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.10
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(juce)

# ------------------------------------------------------------------------------------------------
# SQLite (public domain), to read preset databases (Arturia, Steinberg MediaBay...) read-only
# ------------------------------------------------------------------------------------------------
FetchContent_Declare(sqlite
    URL      https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
    URL_HASH SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(sqlite)

add_library(ss_sqlite STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
target_include_directories(ss_sqlite PUBLIC ${sqlite_SOURCE_DIR})
target_compile_definitions(ss_sqlite PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
    SQLITE_DQS=0)
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(ss_sqlite PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

# ------------------------------------------------------------------------------------------------
# Plug-in + standalone app
# ------------------------------------------------------------------------------------------------
set(SS_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND SS_FORMATS AU)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SS_FORMATS LV2)
endif()

juce_add_plugin(SongSavant
    COMPANY_NAME                 "Grabber Audio"
    COMPANY_WEBSITE              "https://github.com/DK7602/Song-Savant"
    PRODUCT_NAME                 "Song Savant"
    BUNDLE_ID                    com.grabberaudio.songsavant
    PLUGIN_MANUFACTURER_CODE     Grbr
    PLUGIN_CODE                  SgSv
    FORMATS                      ${SS_FORMATS}
    IS_SYNTH                     TRUE
    NEEDS_MIDI_INPUT             TRUE
    NEEDS_MIDI_OUTPUT            TRUE
    IS_MIDI_EFFECT               FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS  TRUE
    COPY_PLUGIN_AFTER_BUILD      FALSE
    VST3_CATEGORIES              Instrument Tools
    AU_MAIN_TYPE                 kAudioUnitType_MusicDevice
    LV2URI                       "https://github.com/DK7602/Song-Savant"
    ICON_BIG                     "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png")

juce_add_binary_data(SongSavantBinaryData
    HEADER_NAME SSBinaryData.h
    NAMESPACE   SSBinary
    SOURCES
        Resources/owl.png
        Resources/wordmark.png
        Resources/owl-light.png
        Resources/wordmark-light.png)

set(SS_CORE_SOURCES
    Source/core/Paths.cpp
    Source/core/SystemLocations.cpp
    Source/core/FileWalk.cpp
    Source/core/PluginScanner.cpp
    Source/core/VstPreset.cpp
    Source/core/VstSoundProbe.cpp
    Source/core/SqliteProbe.cpp
    Source/core/VendorProbe.cpp
    Source/core/Siblings.cpp
    Source/core/SetupScan.cpp
    Source/core/Report.cpp
    Source/core/MidiClip.cpp
    Source/core/DropInfo.cpp
    Source/core/SoundKinds.cpp
    Source/core/Database.cpp
    Source/core/Catalog.cpp
    Source/core/Music.cpp
    Source/core/Parts.cpp
    Source/core/Palette.cpp
    Source/core/SongSession.cpp
    Source/core/PluginProfiles.cpp
    Source/core/PluginTypes.cpp
    Source/core/ScanJob.cpp)

set(SS_UI_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/Theme.cpp
    Source/ui/Widgets.cpp
    Source/ui/SetupCheckPage.cpp
    Source/ui/LibraryPage.cpp
    Source/ui/SongPage.cpp
    Source/ui/MainView.cpp)

add_library(SSShared INTERFACE)
target_include_directories(SSShared INTERFACE Source)
target_link_libraries(SSShared INTERFACE
    SongSavantBinaryData
    ss_sqlite
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

set(SS_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(SS_LINUX_MINIMAL "Disable ALSA/JACK/Xrandr/Xinerama/Xcursor (CI / headless builds)" OFF)
    if(SS_LINUX_MINIMAL)
        list(APPEND SS_DEFS JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
    endif()
endif()

target_compile_definitions(SSShared INTERFACE ${SS_DEFS} SS_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(SSShared INTERFACE /bigobj /utf-8)
endif()

target_sources(SongSavant PRIVATE ${SS_CORE_SOURCES} ${SS_UI_SOURCES})
target_link_libraries(SongSavant PRIVATE SSShared)
target_compile_definitions(SongSavant PUBLIC JUCE_MODAL_LOOPS_PERMITTED=0)

# ------------------------------------------------------------------------------------------------
# Song Savant Scanner: asks one plug-in what it is (run by Song Savant in its own process, so a crashing plug-in
# can't take the app or the DAW down). Installed next to the standalone app.
# ------------------------------------------------------------------------------------------------
juce_add_console_app(SongSavantScanner PRODUCT_NAME "Song Savant Scanner")
target_sources(SongSavantScanner PRIVATE Source/scanner/ScannerMain.cpp)
set_target_properties(SongSavantScanner PROPERTIES OUTPUT_NAME "Song Savant Scanner")
target_compile_definitions(SongSavantScanner PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0 JUCE_PLUGINHOST_LADSPA=0
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(SongSavantScanner PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
if(SS_LINUX_MINIMAL)
    target_compile_definitions(SongSavantScanner PRIVATE JUCE_ALSA=0 JUCE_JACK=0)
endif()

# ------------------------------------------------------------------------------------------------
# Headless test runner: builds a fake computer, scans it, checks the results, draws the screen
# ------------------------------------------------------------------------------------------------
if(SS_BUILD_TESTS)
    juce_add_console_app(SSTests PRODUCT_NAME "SSTests")
    target_sources(SSTests PRIVATE Tests/TestMain.cpp Tests/FakeComputer.cpp ${SS_CORE_SOURCES} ${SS_UI_SOURCES})
    target_compile_definitions(SSTests PRIVATE SS_TEST_RUNNER=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SSTests PRIVATE SSShared)
    # the tests ask the real scanner about the real plug-in
    add_dependencies(SSTests SongSavantScanner SongSavant_VST3)
    target_compile_definitions(SSTests PRIVATE
        SS_TEST_SCANNER="$<TARGET_FILE:SongSavantScanner>"
        SS_TEST_VST3="${CMAKE_BINARY_DIR}/SongSavant_artefacts/$<CONFIG>/VST3/Song Savant.vst3")
endif()
