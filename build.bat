@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

set BUILD_DIR=build
set CONFIG=Release

echo ===============================================
echo  Building SmoothNativeChat using CMake (Win32 x86)
echo ===============================================

if not exist %BUILD_DIR% (
    echo [INFO] Creating build direcroty: %BUILD_DIR%...
    mkdir %BUILD_DIR%
)

cd %BUILD_DIR%

echo [INFO] Try to configure CMake...
cmake -G "Visual Studio 17 2022" -A Win32 ..
if %errorlevel% neq 0 (
    echo [ERROR] Configuration problem!
    pause
    exit /b %errorlevel%
)

echo [INFO] Compiling project (%CONFIG%)...
cmake --build . --config %CONFIG%
if %errorlevel% neq 0 (
    echo [ERROR] Compilation problem!
    pause
    exit /b %errorlevel%
)

echo ===============================================
echo [SUCCESS] Project has been builed!
echo Plugin .asi now in game folder.
echo ===============================================
pause
