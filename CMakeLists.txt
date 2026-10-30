cmake_minimum_required(VERSION 3.21)

project(SmoothNativeChat LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This project can only be compiled on Windows.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build only with Win32/x86 toolchain. SAMP/GTA SA is a 32-bit application. Please use -A Win32")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#
#   MinHook (submodule)
#

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/MinHook/CMakeLists.txt")
    set(MINHOOK_BUILD_SAMPLES OFF CACHE BOOL "" FORCE) 
    add_subdirectory(third-party/MinHook)
else()
    message(FATAL_ERROR "MinHook submodule not found! Run: git submodule update --init --recursive")
endif()

#
# ImGui (submodule)
#

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui")

if(EXISTS "${IMGUI_DIR}/imgui.cpp")
    set(IMGUI_SOURCES
        "${IMGUI_DIR}/imgui.cpp"
        "${IMGUI_DIR}/imgui_draw.cpp"
        "${IMGUI_DIR}/imgui_tables.cpp"
        "${IMGUI_DIR}/imgui_widgets.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_dx9.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    )
    
    add_library(imgui STATIC ${IMGUI_SOURCES})
    
    target_include_directories(imgui PUBLIC 
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
else()
    message(FATAL_ERROR "ImGui submodule not found! Run: git submodule update --init --recursive")
endif()

# github actions path hot-fix (becase actions uses nuget d3d9 package)
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
)

add_library(${PROJECT_NAME} MODULE ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    SUFFIX ".asi"
    PREFIX ""
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party
    "$ENV{DXSDK_DIR}/Include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32 _CONSOLE NOMINMAX _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES _MBCS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /MP /Zc:strictStrings-)
endif()

# DirectX library paths, depends on environment: local or actions (NuGet package)
if(DXSDK_LIB_DIR)
    target_link_directories(${PROJECT_NAME} PRIVATE "${DXSDK_LIB_DIR}")
else()
    target_link_directories(${PROJECT_NAME} PRIVATE "${DXSDK_DIR}/Lib/x86")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    minhook
    imgui    
    d3d9
    d3dx9
    dsound
    dxguid
    winmm
    comctl32
)

set(GTA_DIR "G:/GTA San Andreas - SAMP" CACHE PATH "Path to GTA San Andreas directory")

if(EXISTS "${GTA_DIR}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${GTA_DIR}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMENT "Copying ASI plugin to GTA SA directory..."
    )
else()
    message(STATUS "GTA SA directory not found. Skipping automatic post-build copy.")
endif()