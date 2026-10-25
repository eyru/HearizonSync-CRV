cmake_minimum_required(VERSION 3.20)
project(HearizonSyncCRV VERSION 2.5.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 统一 UTF-8 源码与可执行文件
if(MSVC)
    add_compile_options(/utf-8 /W3 /MP /permissive-)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    add_compile_options(-Wall -Wextra -O2)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set(HS_THIRD_PARTY "${CMAKE_SOURCE_DIR}/third_party")

# ---------------------------------------------------------------- 核心库
set(HS_SOURCES
    src/core/Json.cpp
    src/core/Util.cpp
    src/core/Http.cpp
    src/core/Config.cpp
    src/core/Theme.cpp
    src/core/Strings.cpp
    src/audio/AudioCapture.cpp
    src/asr/Recognizer.cpp
    src/mt/Translator.cpp
    src/mt/LocalTranslator.cpp
    src/mt/FreeOnlineTranslator.cpp
    src/mt/ApiTranslator.cpp
    src/platform/Tray.cpp
    src/platform/Updater.cpp
    src/platform/SingleInstance.cpp
    src/ui/Widgets.cpp
    src/ui/MainWindow.cpp
    src/ui/Dialogs.cpp
    src/ui/Eggs.cpp
)

add_library(hearsync_core STATIC ${HS_SOURCES})
target_include_directories(hearsync_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${HS_THIRD_PARTY}/sherpa
    ${HS_THIRD_PARTY}/ctranslate2
)

# sherpa-onnx C API
if(MSVC)
    target_link_libraries(hearsync_core PUBLIC
        ${HS_THIRD_PARTY}/sherpa/sherpa-onnx-c-api.lib
        ${HS_THIRD_PARTY}/ctranslate2/ctranslate2.lib
    )
endif()

target_link_libraries(hearsync_core PUBLIC
    winhttp ole32 oleaut32 uuid shell32 comdlg32 gdi32 user32 advapi32
    comctl32 dwmapi shcore version ws2_32
)

# ---------------------------------------------------------------- 主程序
add_executable(HearizonSync src/main.cpp)
target_link_libraries(HearizonSync PRIVATE hearsync_core)

if(MSVC)
    set_target_properties(HearizonSync PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    target_link_options(HearizonSync PRIVATE /SUBSYSTEM:WINDOWS)
    # 版本信息与图标
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/app.rc")
        target_sources(HearizonSync PRIVATE resources/app.rc)
    endif()
endif()

# ---------------------------------------------------------------- 测试
option(HS_BUILD_TESTS "Build tests" ON)
if(HS_BUILD_TESTS)
    enable_testing()
    foreach(t test_json test_util test_http test_config test_vad test_asr test_mt)
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/${t}.cpp")
            add_executable(${t} tests/${t}.cpp)
            target_link_libraries(${t} PRIVATE hearsync_core)
            add_test(NAME ${t} COMMAND ${t})
        endif()
    endforeach()
endif()
