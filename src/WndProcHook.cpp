#include "WndProcHook.h"
#include "Globals.h"
#include "Logger.h"
#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>

// SA-MP R5 Addresses
static constexpr uintptr_t kGamePtrOffset = 0x26EBAC;
static constexpr uintptr_t kSetCursorMode = 0xA06F0;
static constexpr uintptr_t kUnlockActorCam = 0xA05D0;

// ������ ������������� ������� SA-MP.
// state = true  -> ��������� ����� (������ �� �������� �����)
// state = false -> ������� ����� (������ ����������� �����)
void ToggleCursor(bool state) {
    void* game = *reinterpret_cast<void**>(g_samp_base + kGamePtrOffset);

    if (!game) {
        return;
    }

    auto set_cursor_mode = reinterpret_cast<void(__thiscall*)(void*, int, BOOL)>(g_samp_base + kSetCursorMode);
    set_cursor_mode(game, state ? 3 : 0, state ? FALSE : TRUE);

    if (!state) {
        auto unlock_cam = reinterpret_cast<void(__thiscall*)(void*)>(g_samp_base + kUnlockActorCam);
        unlock_cam(game);
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static WNDPROC g_original_wndproc = nullptr;

extern bool g_menu_open;
bool g_cursor_shown;

static LRESULT __stdcall HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // ���� ���� ������� � ���� ������ ������ � ImGui.
    if (g_menu_open) {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        return TRUE;
    }

    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_menu_open = !g_menu_open;
        return true;
    }

    // ���� ������� � SA-MP ��� ��������� �������� � �������.
    return CallWindowProcA(g_original_wndproc, hWnd, msg, wParam, lParam);
}

void InstallWndProcHook() {
    HWND game_window = **reinterpret_cast<HWND**>(0xC17054);

    if (!game_window) {
        Logger::Error("Game window not found for WndProc hook");
        return;
    }

    g_original_wndproc =
        reinterpret_cast<WNDPROC>(SetWindowLongA(game_window, GWL_WNDPROC, reinterpret_cast<LONG>(HookedWndProc)));

    Logger::Ok("WndProc hook installed");
}