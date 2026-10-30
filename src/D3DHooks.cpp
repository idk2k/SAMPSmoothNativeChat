#include "D3DHooks.h"
#include "Globals.h"
#include "Config.h"
#include "Logger.h"
#include "ChatRenderer.h"

#include <Windows.h>
#include <MinHook.h>
#include <string>

#include "Menu.h"
#include "WndProcHook.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui/imgui_impl_win32.h>

// ---------------------------------------------------------------------------
// IDirect3DDevice9::EndScene
// ---------------------------------------------------------------------------

using EndScene_t = HRESULT(__stdcall*)(IDirect3DDevice9* device);

static EndScene_t fp_end_scene = nullptr;
static bool g_imgui_ready = false;

extern bool g_menu_open;
extern bool g_cursor_shown;
extern void ToggleCursor(bool state);

static HRESULT __stdcall EndSceneHooked(IDirect3DDevice9* device) {
    g_device = device;

    if (!g_graphics_ready) {
        CreateGraphics(device);
    }

    // ����������� ������������� ImGui.
    if (!g_imgui_ready) {
        menu::Init(device);
        InstallWndProcHook();
        g_imgui_ready = true;
    }

    // --- ImGui frame ---
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // � ������ Present, ����� ImGui::NewFrame():
    if (g_menu_open) {
        if (!g_cursor_shown) {
            g_cursor_shown = true;
            ToggleCursor(true);
        }
        // ... ��������� ���� ...
    }
    else if (g_cursor_shown) {
        g_cursor_shown = false;
        ToggleCursor(false);
    }

    // --- ��� ��� (��� ����) ---
    RECT clip;
    clip.left = g_config.chat.x;
    clip.top = g_config.chat.y;
    clip.right = g_config.chat.x + g_config.chat.width;
    clip.bottom = g_config.chat.y + g_config.chat.height;

    device->SetScissorRect(&clip);

    DWORD old_scissor = 0;
    device->GetRenderState(D3DRS_SCISSORTESTENABLE, &old_scissor);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

    g_scroll.Update(g_frame_clock.Tick());
    RenderChat(device);

    device->SetRenderState(D3DRS_SCISSORTESTENABLE, old_scissor);

    // --- ���� ImGui (������ ����) ---
    menu::Render(device);

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    if (fp_end_scene) {
        return fp_end_scene(device);
    }

    return S_OK;
}

// ---------------------------------------------------------------------------
// IDirect3DDevice9::Reset
// ---------------------------------------------------------------------------

using Reset_t = HRESULT(__stdcall*)(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* parameters);

static Reset_t fp_reset = nullptr;

static HRESULT __stdcall ResetHooked(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* parameters)
{
    ReleaseGraphics();

    const HRESULT result =
        fp_reset(device, parameters);

    if (SUCCEEDED(result)) {
        g_device = device;
        CreateGraphics(device);
    }

    return result;
}

// ---------------------------------------------------------------------------
// ��������� D3D-�����.
// ---------------------------------------------------------------------------

static bool CreateAndEnableHook(
    const char* name,
    void* target,
    void* detour,
    void** original)
{
    Logger::Pointer(name, reinterpret_cast<uintptr_t>(target));

    MH_STATUS status = MH_CreateHook(
        target,
        detour,
        original);

    if (status != MH_OK) {
        Logger::Error(
            std::string(name) + ": MH_CreateHook failed, status=" +
            std::to_string(static_cast<int>(status)));
        return false;
    }

    status = MH_EnableHook(target);

    if (status != MH_OK) {
        Logger::Error(
            std::string(name) + ": MH_EnableHook failed, status=" +
            std::to_string(static_cast<int>(status)));
        return false;
    }

    Logger::Hook(std::string(name) + " installed");
    return true;
}

bool InstallD3DHooks() {
    const DWORD device_pointer =
        *reinterpret_cast<DWORD*>(0xC97C28);

    if (!device_pointer) {
        Logger::Error("D3D9 device pointer is null");
        return false;
    }

    void** vtable =
        *reinterpret_cast<void***>(device_pointer);

    if (!vtable) {
        Logger::Error("D3D9 vtable is null");
        return false;
    }

    // EndScene � vtable index 42.
    if (!CreateAndEnableHook(
        "IDirect3DDevice9::EndScene",
        vtable[42],
        &EndSceneHooked,
        reinterpret_cast<void**>(&fp_end_scene)))
    {
        return false;
    }

    // Reset � vtable index 16.
    CreateAndEnableHook(
        "IDirect3DDevice9::Reset",
        vtable[16],
        &ResetHooked,
        reinterpret_cast<void**>(&fp_reset));

    return true;
}

void ShutdownD3DHooks() {
    if (fp_end_scene) {
        MH_DisableHook(
            *reinterpret_cast<void**>(
                *reinterpret_cast<void***>(
                    *reinterpret_cast<DWORD*>(0xC97C28)) + 42));
    }

    if (fp_reset) {
        MH_DisableHook(
            *reinterpret_cast<void**>(
                *reinterpret_cast<void***>(
                    *reinterpret_cast<DWORD*>(0xC97C28)) + 16));
    }
}