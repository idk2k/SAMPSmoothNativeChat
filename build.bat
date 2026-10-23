@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ====================================================
echo   Запуск генерации и сборки проекта через CMake
echo ====================================================

:: 1. Проверяем, установлен ли CMake в системе
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ОШИБКА] CMake не найден! Убедитесь, что он добавлен в переменную среды PATH.
    pause
    exit /b 1
)

:: 2. Конфигурация и генерация файлов сборки
echo [1/2] Генерация проектных файлов для Visual Studio 2022 (x86)...
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
if %errorlevel% neq 0 (
    echo [ОШИБКА] Не удалось сгенерировать файлы конфигурации CMake.
    pause
    exit /b %errorlevel%
)

echo.
echo ----------------------------------------------------
echo.

:: 3. Сборка проекта в конфигурации Release
echo [2/2] Компиляция проекта (Конфигурация: Release)...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo [ОШИБКА] Сборка проекта завершилась неудачно.
    pause
    exit /b %errorlevel%
)

echo.
echo ====================================================
echo   [УСПЕХ] Проект успешно собран!
echo ====================================================
pause
endlocal
