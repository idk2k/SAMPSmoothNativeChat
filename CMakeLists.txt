cmake_minimum_required(VERSION 3.21)

project(BezierSAMP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "Build only with Win32/x86 toolchain. SAMP is a 32-bit application."
    )
endif()

set(SOURCES
    src/dllmain.cpp
    src/ConsoleManager.cpp
    src/EasingsHolder.cpp
    src/UtilityHelper.cpp

    src/dxut/dxstdafx.cpp
    src/dxut/DXUT.cpp
    src/dxut/DXUTenum.cpp
    src/dxut/DXUTgui.cpp
    src/dxut/DXUTMesh.cpp
    src/dxut/DXUTmisc.cpp
    src/dxut/DXUTSettingsDlg.cpp
    src/dxut/DXUTsound.cpp

    third-party/imgui/imgui.cpp
    third-party/imgui/imgui_draw.cpp
    third-party/imgui/imgui_impl_dx9.cpp
    third-party/imgui/imgui_impl_win32.cpp
    third-party/imgui/imgui_tables.cpp
    third-party/imgui/imgui_widgets.cpp
)

add_library(BezierSAMP MODULE ${SOURCES})

set_target_properties(BezierSAMP PROPERTIES
    OUTPUT_NAME "BezierSAMP"
    SUFFIX ".asi"
    PREFIX ""
)

target_include_directories(BezierSAMP PRIVATE
    src
    third-party
    third-party/imgui
    "$ENV{DXSDK_DIR}/Include"
)

target_compile_definitions(BezierSAMP PRIVATE
    WIN32
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    _USE_MATH_DEFINES
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

if(MSVC)
    target_compile_options(BezierSAMP PRIVATE
        /W3
        /MP
        /Zc:strictStrings-
        /utf-8
    )

    target_link_options(BezierSAMP PRIVATE
        "/SUBSYSTEM:WINDOWS"
    )
endif()

target_link_directories(BezierSAMP PRIVATE
    "$ENV{DXSDK_DIR}/Lib/x86"
    "${CMAKE_CURRENT_SOURCE_DIR}/third-party/MinHook/lib"
)

target_link_libraries(BezierSAMP PRIVATE
    d3d9
    d3dx9
    dsound
    dxguid
    winmm
    comctl32
    MinHook.x86
)