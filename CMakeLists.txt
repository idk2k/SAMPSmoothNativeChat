cmake_minimum_required(VERSION 3.21)

project(BezierSAMP LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This project can only be compiled on Windows.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build only with Win32/x86 toolchain. SAMP/GTA SA is a 32-bit application. Please use -A Win32")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/MinHook/CMakeLists.txt")
    set(MINHOOK_BUILD_SAMPLES OFF CACHE BOOL "" FORCE) 
    add_subdirectory(third-party/MinHook)
else()
    message(FATAL_ERROR "MinHook submodule not found! Run: git submodule update --init --recursive")
endif()

# if github actions
if(NOT DXSDK_DIR)
    set(DXSDK_DIR "$ENV{DXSDK_DIR}")
endif()

set(SOURCES
    src/dllmain.cpp
    src/ChatHooks.cpp
    src/ChatRenderer.cpp
    src/ConfigLoader.cpp
    src/ConsoleManager.cpp
    src/D3DHooks.cpp
    src/Globals.cpp
    src/Menu.cpp
    src/WndProcHook.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/MinHook/include
    "$ENV{DXSDK_DIR}/Include"
)

target_compile_definitions(BezierSAMP PRIVATE
    WIN32 _CONSOLE NOMINMAX _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES _MBCS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

if(MSVC)
    target_compile_options(BezierSAMP PRIVATE /W3 /MP /Zc:strictStrings-)
endif()

if(DXSDK_LIB_DIR)
    target_link_directories(BezierSAMP PRIVATE "${DXSDK_LIB_DIR}")
else()
    target_link_directories(BezierSAMP PRIVATE "${DXSDK_DIR}/Lib/x86")
endif()

target_link_libraries(BezierSAMP PRIVATE
    minhook    
    d3d9
    d3dx9
    dsound
    dxguid
    winmm
    comctl32
)

set(GTA_DIR "G:/GTA San Andreas - SAMP")

if(EXISTS "${GTA_DIR}")
    add_custom_command(TARGET BezierSAMP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:BezierSAMP>"
            "${GTA_DIR}/$<TARGET_FILE_NAME:BezierSAMP>"
        COMMENT "Copying ASI plugin to GTA SA directory..."
    )
else()
    message(STATUS "GTA SA directory not found. Skipping automatic post-build copy.")
endif()