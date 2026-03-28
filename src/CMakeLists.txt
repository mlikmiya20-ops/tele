cmake_minimum_required(VERSION 3.20)

project(telecloud-multi VERSION 1.00 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Options
option(ENABLE_GUI "Enable Qt Quick GUI" ON)
option(ENABLE_TESTS "Build tests" OFF)

# =============================================================================
# Qt Configuration
# =============================================================================
if(ENABLE_GUI)
    find_package(Qt6 REQUIRED COMPONENTS
        Core
        Quick
        Network
        Concurrent
    )
else()
    find_package(Qt6 REQUIRED COMPONENTS
        Core
        Network
        Concurrent
    )
endif()

# =============================================================================
# FFmpeg Configuration
# =============================================================================
set(FFMPEG_ROOT "" CACHE PATH "Path to FFmpeg installation")
set(FFMPEG_INCLUDE_DIR "" CACHE PATH "Path to FFmpeg include directory")
set(FFMPEG_LIB_DIR "" CACHE PATH "Path to FFmpeg lib directory")

if(FFMPEG_ROOT AND NOT FFMPEG_INCLUDE_DIR)
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
endif()
if(FFMPEG_ROOT AND NOT FFMPEG_LIB_DIR)
    set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
endif()

# Find FFmpeg headers
find_path(AVFORMAT_INCLUDE_DIR
    NAMES libavformat/avformat.h
    PATHS
        ${FFMPEG_INCLUDE_DIR}
        /usr/include
        /usr/local/include
        $ENV{FFMPEG_ROOT}/include
    NO_DEFAULT_PATH
)

if(NOT AVFORMAT_INCLUDE_DIR)
    message(WARNING "FFmpeg headers not found. Build may fail.")
    set(FFMPEG_INCLUDE_DIR "")
else()
    set(FFMPEG_INCLUDE_DIR ${AVFORMAT_INCLUDE_DIR})
    message(STATUS "FFmpeg include: ${FFMPEG_INCLUDE_DIR}")
endif()

# FFmpeg libraries
set(FFMPEG_LIBS
    avformat
    avcodec
    avutil
)

if(NOT ANDROID)
    list(APPEND FFMPEG_LIBS avdevice avfilter swscale swresample)
endif()

set(FFMPEG_LIBRARIES "")

foreach(LIB ${FFMPEG_LIBS})
    if(WIN32)
        find_library(FFMPEG_${LIB}_LIB
            NAMES ${LIB} lib${LIB}
            PATHS
                ${FFMPEG_LIB_DIR}
                ${FFMPEG_ROOT}/lib
            NO_DEFAULT_PATH
        )
    elseif(ANDROID)
        # For Android, FFmpeg is usually .so files
        find_library(FFMPEG_${LIB}_LIB
            NAMES ${LIB}
            PATHS
                ${FFMPEG_LIB_DIR}
                ${FFMPEG_ROOT}/lib
            NO_DEFAULT_PATH
        )
    else()
        find_library(FFMPEG_${LIB}_LIB
            NAMES ${LIB}
            PATHS
                ${FFMPEG_LIB_DIR}
                /usr/lib/x86_64-linux-gnu
                /usr/local/lib
            NO_DEFAULT_PATH
        )
    endif()

    if(FFMPEG_${LIB}_LIB)
        list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${LIB}_LIB})
        message(STATUS "Found FFmpeg ${LIB}: ${FFMPEG_${LIB}_LIB}")
    else()
        message(STATUS "FFmpeg ${LIB} not found (optional)")
    endif()
endforeach()

# =============================================================================
# Android Configuration
# =============================================================================
if(ANDROID)
    set(ANDROID_MIN_SDK_VERSION 26 CACHE STRING "Minimum Android SDK version")
    set(ANDROID_TARGET_SDK_VERSION 34 CACHE STRING "Target Android SDK version")
    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../android" CACHE INTERNAL "")
endif()

# =============================================================================
# Windows Configuration
# =============================================================================
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# =============================================================================
# Source Files
# =============================================================================
set(CORE_SOURCES
    core/errorlogger.h
    core/networkscanner.h
    core/networkscanner.cpp
    core/rtspprobe.h
    core/rtspprobe.cpp
    core/recorder.h
    core/recorder.cpp
    core/dvrmanager.h
    core/dvrmanager.cpp
)

if(ENABLE_GUI)
    # QML is embedded in resources.qrc
    set(RESOURCES resources.qrc)
    set(GUI_SOURCES main.cpp ${RESOURCES})
else()
    set(GUI_SOURCES console_main.cpp)
endif()

# =============================================================================
# Executable
# =============================================================================
if(ANDROID AND ENABLE_GUI)
    # For Android with GUI, use qt_add_executable
    qt_add_executable(telecloud-multi
        ${CORE_SOURCES}
        ${GUI_SOURCES}
    )
elseif(ENABLE_GUI)
    qt_add_executable(telecloud-multi
        ${CORE_SOURCES}
        ${GUI_SOURCES}
    )
else()
    add_executable(telecloud-multi
        ${CORE_SOURCES}
        ${GUI_SOURCES}
    )
endif()

# =============================================================================
# Windows GUI Application (no console window)
# =============================================================================
if(WIN32 AND ENABLE_GUI)
    # This prevents the console window from appearing on Windows
    set_target_properties(telecloud-multi PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(telecloud-multi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

if(FFMPEG_INCLUDE_DIR)
    target_include_directories(telecloud-multi PRIVATE ${FFMPEG_INCLUDE_DIR})
endif()

# =============================================================================
# Link Libraries
# =============================================================================
if(ENABLE_GUI)
    target_link_libraries(telecloud-multi PRIVATE
        Qt6::Core
        Qt6::Quick
        Qt6::Network
        Qt6::Concurrent
    )
else()
    target_link_libraries(telecloud-multi PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Concurrent
    )
endif()

# Link FFmpeg
if(FFMPEG_LIBRARIES)
    target_link_libraries(telecloud-multi PRIVATE ${FFMPEG_LIBRARIES})
endif()

# FFmpeg library directories
if(FFMPEG_LIB_DIR AND ANDROID)
    target_link_directories(telecloud-multi PRIVATE ${FFMPEG_LIB_DIR})
endif()

# =============================================================================
# Compiler Definitions
# =============================================================================
target_compile_definitions(telecloud-multi PRIVATE
    QT_MESSAGELOGCONTEXT
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

if(NOT ENABLE_GUI)
    target_compile_definitions(telecloud-multi PRIVATE TELECLOUD_CONSOLE)
endif()

# =============================================================================
# Platform-specific Settings
# =============================================================================
if(ANDROID)
    target_compile_definitions(telecloud-multi PRIVATE Q_OS_ANDROID)

elseif(WIN32)
    target_compile_definitions(telecloud-multi PRIVATE Q_OS_WINDOWS)

elseif(UNIX)
    target_compile_definitions(telecloud-multi PRIVATE Q_OS_LINUX)
endif()

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "=== TELECLOUD-MULTI ${PROJECT_VERSION} ===")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "GUI: ${ENABLE_GUI}")
if(Qt6_VERSION)
    message(STATUS "Qt: ${Qt6_VERSION}")
endif()
message(STATUS "FFmpeg include: ${FFMPEG_INCLUDE_DIR}")
message(STATUS "FFmpeg libs: ${FFMPEG_LIBRARIES}")
if(ANDROID)
    message(STATUS "Android ABI: ${ANDROID_ABI}")
    message(STATUS "Android Min SDK: ${ANDROID_MIN_SDK_VERSION}")
endif()
message(STATUS "========================================")
message(STATUS "")
