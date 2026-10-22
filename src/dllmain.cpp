#define _CRT_SECURE_NO_WARNINGS
#define BUILD_WINDOWS
#define NOMINMAX

#include <Windows.h>
#include <intrin.h>
#include <tchar.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

#include "ConsoleManager.h"
#include "EasingsHolder.h"
#include "MinHook/MinHook.h"
#include "Structures.h"
#include "UtilityHelper.h"

#include "d3d9.h"
#include "d3dx9core.h"
#include "dxut/dxstdafx.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

#pragma execution_character_set("utf-8")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);

#pragma intrinsic(_ReturnAddress)

// ============================================================
// Global state
// ============================================================

bool initialize = false;
bool bMenuOpen = false;
bool bCursorShown = false;

WNDPROC m_pWindowProc = nullptr;

using ProtPresent = HRESULT(__stdcall*)(
    IDirect3DDevice9*,
    CONST RECT*,
    CONST RECT*,
    HWND,
    CONST RGNDATA*
);

using ProtReset = HRESULT(__stdcall*)(
    IDirect3DDevice9*,
    D3DPRESENT_PARAMETERS*
);

using DrawSpriteFP = HRESULT(__stdcall*)(
    ID3DXSprite*,
    LPDIRECT3DTEXTURE9,
    const RECT*,
    const D3DXVECTOR3*,
    const D3DXVECTOR3*,
    D3DCOLOR
);

ProtReset Reset = nullptr;
ProtPresent Present = nullptr;
DrawSpriteFP DrawSprite = nullptr;

HRESULT __stdcall Hooked_Present(
    IDirect3DDevice9* pDevice,
    CONST RECT* pSrcRect,
    CONST RECT* pDestRect,
    HWND hDestWindow,
    CONST RGNDATA* pDirtyRegion
);

HRESULT __stdcall Hooked_Reset(
    LPDIRECT3DDEVICE9 pDevice,
    D3DPRESENT_PARAMETERS* pPresentParams
);

HRESULT __stdcall Hooked_DrawSprite(
    ID3DXSprite* sprite,
    LPDIRECT3DTEXTURE9 pTexture,
    const RECT* pSrcRect,
    const D3DXVECTOR3* pCenter,
    const D3DXVECTOR3* pPosition,
    D3DCOLOR Color
);

// ============================================================
// SAMP addresses
// ============================================================

DWORD SAMP_GAME_PTR = 0x21A10C;
DWORD SAMP_FUNC_ADDCLIENTCMD = 0x65AD0;
DWORD SAMP_FUNC_TOGGLECURSOR = 0x9BD30;
DWORD SAMP_FUNC_CURSORUNLOCKACTORCAM = 0x9BC10;

bool ToggleCursor(bool state);

// ============================================================
// D3D9 / ImGui
// ============================================================

HRESULT __stdcall Hooked_Present(
    IDirect3DDevice9* pDevice,
    CONST RECT* pSrcRect,
    CONST RECT* pDestRect,
    HWND hDestWindow,
    CONST RGNDATA* pDirtyRegion
) {
    static bool once = false;

    if (!once) {
        ImGui::CreateContext();

        ImGui_ImplWin32_Init(**reinterpret_cast<HWND**>(0xC17054));

#pragma warning(push)
#pragma warning(disable : 4996)

        std::string font = getenv("WINDIR");
        font += "\\Fonts\\Tahoma.TTF";

#pragma warning(pop)

        ImGui::GetIO().Fonts->AddFontFromFileTTF(
            font.c_str(),
            14.0f,
            nullptr,
            ImGui::GetIO().Fonts->GetGlyphRangesCyrillic()
        );

        ImGui_ImplDX9_Init(pDevice);

        once = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (bMenuOpen) {
        if (!bCursorShown) {
            bCursorShown = ToggleCursor(true);
        }

        const int screenX = *reinterpret_cast<int*>(0x00C17044);
        const int screenY = *reinterpret_cast<int*>(0x00C17048);

        ImGui::SetNextWindowPos(
            ImVec2(
                static_cast<float>(screenX) / 2.0f,
                static_cast<float>(screenY) / 2.0f
            ),
            ImGuiCond_FirstUseEver
        );

        ImGui::Begin(
            "SAMP Chat Scroll",
            &bMenuOpen,
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse
        );

        DrawScrollSettings();

        ImGui::Separator();

        if (ImGui::Button("Reset scroll")) {
            g_scroll.Reset();
        }

        ImGui::End();
    } else if (bCursorShown) {
        bCursorShown = ToggleCursor(false);
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return Present(
        pDevice,
        pSrcRect,
        pDestRect,
        hDestWindow,
        pDirtyRegion
    );
}

HRESULT __stdcall Hooked_Reset(
    LPDIRECT3DDEVICE9 pDevice,
    D3DPRESENT_PARAMETERS* pPresentParams
) {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    return Reset(pDevice, pPresentParams);
}

struct Addresses {
    std::uintptr_t samp_base = 0;
    std::uintptr_t g_chat = 0;
    std::uintptr_t rakpeer_connect = 0;
} addresses;

HRESULT __stdcall Hooked_DrawSprite(
    ID3DXSprite* sprite,
    LPDIRECT3DTEXTURE9 pTexture,
    const RECT* pSrcRect,
    const D3DXVECTOR3* pCenter,
    const D3DXVECTOR3* pPosition,
    D3DCOLOR Color
) {
    static RECT rectsrc{};
    static bool once = false;
    static D3DXVECTOR3 rect_test{};

    if (_ReturnAddress() ==
        reinterpret_cast<void*>(addresses.samp_base + 0x642C9)) {

        if (!once) {
            rectsrc.right = 0x00FFFFFF;
            rectsrc.bottom = 0x10D;
            rectsrc.top = 0x1E;

            rect_test.x = 0.0f;
            rect_test.y = 10.0f;
            rect_test.z = 0.0f;

            once = true;
        }

        return DrawSprite(
            sprite,
            pTexture,
            &rectsrc,
            pCenter,
            &rect_test,
            Color
        );
    }

    return DrawSprite(
        sprite,
        pTexture,
        pSrcRect,
        pCenter,
        pPosition,
        Color
    );
}

void InstallD3DHook() {
    const DWORD pDevice = *reinterpret_cast<DWORD*>(0xC97C28);
    void** vTable = *reinterpret_cast<void***>(pDevice);

    void** vTable2 = *reinterpret_cast<void***>(
        *reinterpret_cast<DWORD*>(
            reinterpret_cast<char*>(addresses.g_chat) + 0x63AA
        )
    );

    MH_CreateHook(
        vTable[17],
        Hooked_Present,
        reinterpret_cast<LPVOID*>(&Present)
    );

    MH_EnableHook(vTable[17]);

    MH_CreateHook(
        vTable[16],
        Hooked_Reset,
        reinterpret_cast<LPVOID*>(&Reset)
    );

    MH_EnableHook(vTable[16]);

    MH_CreateHook(
        vTable2[9],
        Hooked_DrawSprite,
        reinterpret_cast<LPVOID*>(&DrawSprite)
    );

    MH_EnableHook(vTable2[9]);
}

bool ToggleCursor(bool state) {
    void* object = *reinterpret_cast<void**>(
        addresses.samp_base + SAMP_GAME_PTR
    );

    reinterpret_cast<void(__thiscall*)(void*, int, bool)>(
        addresses.samp_base + SAMP_FUNC_TOGGLECURSOR
    )(object, state ? 3 : 0, !state);

    if (!state) {
        reinterpret_cast<void(__thiscall*)(void*)>(
            addresses.samp_base + SAMP_FUNC_CURSORUNLOCKACTORCAM
        )(object);
    }

    return state;
}

// ============================================================
// Frame clock
// ============================================================

class FrameClock {
public:
    FrameClock() {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);

        m_frequency = static_cast<double>(frequency.QuadPart);

        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);

        m_previous = counter.QuadPart;
    }

    float Tick() {
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);

        const double delta =
            static_cast<double>(counter.QuadPart - m_previous) /
            m_frequency;

        m_previous = counter.QuadPart;

        const double clamped = std::min(delta, 0.05);

        return static_cast<float>(clamped);
    }

private:
    double m_frequency = 1.0;
    LONGLONG m_previous = 0;
};

FrameClock g_frameClock;

// ============================================================
// Smooth inertia scroll
// ============================================================

class ChatScrollPhysics {
public:
    void AddImpulse(int direction, float power) {
        if (direction == 0) {
            return;
        }

        const float impulse =
            static_cast<float>(direction) * power;

        const bool directionChanged =
            velocity != 0.0f &&
            std::signbit(velocity) != std::signbit(impulse);

        if (directionChanged) {
            velocity *= reverseBrake;
        }

        targetOffset += impulse;
        velocity += impulse * response;

        active = true;
    }

    void Update(
        float deltaTime,
        float minPosition,
        float maxPosition
    ) {
        if (!active) {
            return;
        }

        const float target = targetOffset;
        const float distance = target - position;

        const bool reverseDirection =
            velocity != 0.0f &&
            std::signbit(velocity) != std::signbit(distance);

        if (reverseDirection) {
            const float brake =
                1.0f - reverseBrake *
                std::min(1.0f, deltaTime * 60.0f);

            velocity *= brake;

            if (std::abs(velocity) <
                std::abs(distance) * 0.12f) {
                velocity = distance * response * 60.0f;
            }
        }

        velocity += distance * stiffness * deltaTime;

        velocity *= std::pow(damping, deltaTime * 60.0f);

        position += velocity * deltaTime;

        if (!isHovering) {
            velocity *= std::pow(inertia, deltaTime * 60.0f);
        }

        if (position < minPosition) {
            position += (minPosition - position) * 0.25f;
            velocity *= 0.55f;
        } else if (position > maxPosition) {
            position += (maxPosition - position) * 0.25f;
            velocity *= 0.55f;
        }

        const bool settled =
            std::abs(distance) < 0.05f &&
            std::abs(velocity) < 0.05f;

        if (settled) {
            position = target;
            velocity = 0.0f;
            active = false;
        }
    }

    float GetPosition() const {
        return position;
    }

    bool IsActive() const {
        return active;
    }

    void Reset() {
        position = 0.0f;
        velocity = 0.0f;
        targetOffset = 0.0f;
        active = false;
    }

    float wheelPower = 1.35f;
    float response = 0.045f;
    float stiffness = 14.0f;
    float damping = 0.82f;
    float inertia = 0.94f;
    float reverseBrake = 0.35f;

    bool isHovering = true;

private:
    float position = 0.0f;
    float velocity = 0.0f;
    float targetOffset = 0.0f;
    bool active = false;
};

ChatScrollPhysics g_scroll;

void DrawScrollSettings() {
    ImGui::Text("Scroll physics");

    ImGui::SliderFloat(
        "Wheel power",
        &g_scroll.wheelPower,
        0.4f,
        3.0f,
        "%.2f"
    );

    ImGui::SliderFloat(
        "Response",
        &g_scroll.response,
        0.0f,
        0.12f,
        "%.3f"
    );

    ImGui::SliderFloat(
        "Stiffness",
        &g_scroll.stiffness,
        4.0f,
        35.0f,
        "%.1f"
    );

    ImGui::SliderFloat(
        "Damping",
        &g_scroll.damping,
        0.60f,
        0.95f,
        "%.3f"
    );

    ImGui::SliderFloat(
        "Inertia",
        &g_scroll.inertia,
        0.75f,
        0.99f,
        "%.3f"
    );

    ImGui::SliderFloat(
        "Reverse brake",
        &g_scroll.reverseBrake,
        0.05f,
        0.90f,
        "%.2f"
    );

    ImGui::Separator();

    if (ImGui::Button("Slow")) {
        g_scroll.wheelPower = 0.70f;
        g_scroll.response = 0.020f;
        g_scroll.stiffness = 7.0f;
        g_scroll.damping = 0.72f;
        g_scroll.inertia = 0.86f;
        g_scroll.reverseBrake = 0.20f;
    }

    ImGui::SameLine();

    if (ImGui::Button("Medium")) {
        g_scroll.wheelPower = 1.35f;
        g_scroll.response = 0.045f;
        g_scroll.stiffness = 14.0f;
        g_scroll.damping = 0.82f;
        g_scroll.inertia = 0.94f;
        g_scroll.reverseBrake = 0.35f;
    }

    ImGui::SameLine();

    if (ImGui::Button("Fast")) {
        g_scroll.wheelPower = 2.20f;
        g_scroll.response = 0.085f;
        g_scroll.stiffness = 26.0f;
        g_scroll.damping = 0.90f;
        g_scroll.inertia = 0.975f;
        g_scroll.reverseBrake = 0.65f;
    }
}

// ============================================================
// SAMP chat helpers
// ============================================================

DWORD& get_min_scrollbar_position_index() {
    return *reinterpret_cast<DWORD*>(
        reinterpret_cast<char*>(
            *reinterpret_cast<void**>(
                reinterpret_cast<char*>(addresses.g_chat) + 0x11E
            )
        ) + 150
    );
}

void move_scrollbar_pointer(int offset) {
    void* scrollbar = *reinterpret_cast<void**>(
        reinterpret_cast<char*>(addresses.g_chat) + 0x11E
    );

    reinterpret_cast<void(__thiscall*)(void*, int)>(
        addresses.samp_base + 0x84190
    )(scrollbar, offset);
}

DWORD get_scrollbar_pointer_pos() {
    return *reinterpret_cast<DWORD*>(
        reinterpret_cast<char*>(
            *reinterpret_cast<void**>(
                reinterpret_cast<char*>(addresses.g_chat) + 0x11E
            )
        ) + 142
    );
}

// ============================================================
// SAMP hooks
// ============================================================

using Scroll_t = void(__fastcall*)(void*, void*, int);
Scroll_t fpScroll = nullptr;

auto __fastcall ScrollHooked(
    void* ecx,
    void* edx,
    int nDelta
) -> void {
    // SAMP обычно передаёт 1 или -1.
    // Если в вашей сборке приходят 120 / -120, этот код всё равно
    // корректно определит направление.
    const int direction = (nDelta > 0) ? 1 : -1;

    g_scroll.AddImpulse(direction, g_scroll.wheelPower);

    fpScroll(ecx, edx, nDelta);
}

using RecalcFontSize_t = void(__fastcall*)(void*, void*);
RecalcFontSize_t fpRecalcFontSize = nullptr;

auto __fastcall RecalcFontSizeHooked(
    void* ecx,
    void* edx
) -> void {
    fpRecalcFontSize(ecx, edx);
}

using AddEntry_t = void(__fastcall*)(
    void*,
    void*,
    int,
    const char*,
    const char*,
    structures::D3DCOLOR,
    structures::D3DCOLOR
);

AddEntry_t fpAddEntry = nullptr;

auto __fastcall AddEntryHooked(
    void* ecx,
    void* edx,
    int nType,
    const char* szText,
    const char* szPrefix,
    structures::D3DCOLOR textColor,
    structures::D3DCOLOR prefixColor
) -> void {
    static int startMin = 101 - 8;

    if (!addresses.g_chat) {
        utils::protect_safe_memory_copy(
            &addresses.g_chat,
            reinterpret_cast<void*>(
                addresses.samp_base + 0x21A0E4
            ),
            0x4
        );
    }

    startMin -= 1;

    get_min_scrollbar_position_index() =
        startMin >= 1
            ? std::min(0x57, startMin)
            : 1;

    fpAddEntry(
        ecx,
        edx,
        nType,
        szText,
        szPrefix,
        textColor,
        prefixColor
    );
}

using Render_t = void(__fastcall*)(void*, void*);
Render_t fpRender = nullptr;

auto __fastcall RenderHooked(
    void* ecx,
    void* edx
) -> void {
    fpRender(ecx, edx);

    const float deltaTime = g_frameClock.Tick();

    const float currentPosition =
        static_cast<float>(get_scrollbar_pointer_pos());

    const float minPosition =
        static_cast<float>(get_min_scrollbar_position_index());

    const float maxPosition = 87.0f;

    g_scroll.Update(
        deltaTime,
        minPosition,
        maxPosition
    );

    if (!g_scroll.IsActive()) {
        return;
    }

    const float scrollPosition = g_scroll.GetPosition();

    const int targetInteger =
        static_cast<int>(std::round(scrollPosition));

    const int currentInteger =
        static_cast<int>(currentPosition);

    const int difference =
        targetInteger - currentInteger;

    if (difference != 0) {
        move_scrollbar_pointer(difference);
    }
}

using RenderEntry_t = void(__fastcall*)(
    void*,
    void*,
    const char*,
    structures::CRect,
    structures::D3DCOLOR
);

RenderEntry_t fpRenderEntry = nullptr;

auto __fastcall RenderEntryHooked(
    void* ecx,
    void* edx,
    const char* szText,
    structures::CRect rect,
    structures::D3DCOLOR color
) -> void {
    if (g_scroll.IsActive()) {
        const float position = g_scroll.GetPosition();

        const float fractional =
            position - std::floor(position);

        // Высота одной строки чата.
        // Если визуально прокрутка слишком маленькая или слишком большая,
        // измените только это значение.
        constexpr float kChatRowHeight = 20.0f;

        // Если направление окажется обратным,
        // замените += на -=.
        rect.top += static_cast<long>(
            fractional * kChatRowHeight
        );
    }

    fpRenderEntry(ecx, edx, szText, rect, color);
}

// ============================================================
// Window procedure
// ============================================================

LRESULT __stdcall WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (msg) {
        case WM_KEYDOWN: {
            if (wParam == VK_F12) {
                bMenuOpen = !bMenuOpen;
            }

            break;
        }

        default:
            break;
    }

    if (bMenuOpen) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
            return true;
        }

        ImGuiIO& io = ImGui::GetIO();

        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            return true;
        }
    }

    return CallWindowProcA(
        m_pWindowProc,
        hWnd,
        msg,
        wParam,
        lParam
    );
}

// ============================================================
// SAMP connection hook
// ============================================================

using RakPeer_Connect = bool(__fastcall*)(
    void*,
    void*,
    const char*,
    unsigned short,
    char*,
    int
);

RakPeer_Connect fpConnect = nullptr;

auto __fastcall RakPeerConnectHooked(
    void* ecx,
    void* edx,
    const char* host,
    unsigned short port,
    char* passwordData,
    int passwordDataLength
) -> bool {
    if (!initialize) {
        m_pWindowProc = reinterpret_cast<WNDPROC>(
            SetWindowLongA(
                *reinterpret_cast<HWND*>(0xC97C1CU),
                GWL_WNDPROC,
                reinterpret_cast<LONG>(WndProcHandler)
            )
        );

        if (!addresses.g_chat) {
            utils::protect_safe_memory_copy(
                &addresses.g_chat,
                reinterpret_cast<void*>(
                    addresses.samp_base + 0x21A0E4
                ),
                0x4
            );
        }

        get_min_scrollbar_position_index() = 0x57;

        InstallD3DHook();

        utils::protect_safe_memory_set(
            reinterpret_cast<void*>(
                addresses.samp_base + 0x6441C + 6U
            ),
            1,
            1
        );

        utils::MH_CreateAndEnableHook(
            static_cast<std::uintptr_t>(
                addresses.samp_base + 0x638A0
            ),
            &RenderEntryHooked,
            reinterpret_cast<LPVOID*>(&fpRenderEntry)
        );

        utils::MH_CreateAndEnableHook(
            static_cast<std::uintptr_t>(
                addresses.samp_base + 0x64010
            ),
            &AddEntryHooked,
            reinterpret_cast<LPVOID*>(&fpAddEntry)
        );

        utils::MH_CreateAndEnableHook(
            static_cast<std::uintptr_t>(
                addresses.samp_base + 0x63550
            ),
            &RecalcFontSizeHooked,
            reinterpret_cast<LPVOID*>(&fpRecalcFontSize)
        );

        utils::MH_CreateAndEnableHook(
            static_cast<std::uintptr_t>(
                addresses.samp_base + 0x637F0
            ),
            &ScrollHooked,
            reinterpret_cast<LPVOID*>(&fpScroll)
        );

        utils::MH_CreateAndEnableHook(
            static_cast<std::uintptr_t>(
                addresses.samp_base + 0x63D70
            ),
            &RenderHooked,
            reinterpret_cast<LPVOID*>(&fpRender)
        );

        initialize = true;
    }

    return fpConnect(
        ecx,
        edx,
        host,
        port,
        passwordData,
        passwordDataLength
    );
}

// ============================================================
// DLL entry
// ============================================================

auto WINAPI DllMain(
    HINSTANCE dllInstance,
    DWORD reason,
    LPVOID reserved
) -> BOOL {
    UNREFERENCED_PARAMETER(reserved);

    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(dllInstance);

            if (ConsoleManager::get_instance().create_console() == FALSE) {
                std::cout <<
                    "[ERROR]: ConsoleManager::get_instance().create_console() failed"
                    << std::endl;

                return FALSE;
            }

            if (const MH_STATUS status = MH_Initialize(); status != MH_OK) {
                std::cout <<
                    "[ERROR]: MH_Initialize() failed, code: "
                    << status << std::endl;

                return FALSE;
            }

            const HMODULE sampModule = GetModuleHandleW(L"samp.dll");

            if (!sampModule) {
                std::cout << "[ERROR]: samp.dll not found" << std::endl;
                return FALSE;
            }

            addresses.samp_base =
                reinterpret_cast<std::uintptr_t>(sampModule);

            addresses.rakpeer_connect = 0x3ABB0;

            utils::MH_CreateAndEnableHook(
                static_cast<std::uintptr_t>(
                    addresses.samp_base + addresses.rakpeer_connect
                ),
                &RakPeerConnectHooked,
                reinterpret_cast<LPVOID*>(&fpConnect)
            );

            break;
        }

        case DLL_PROCESS_DETACH: {
            MH_Uninitialize();
            break;
        }

        default:
            break;
    }

    return TRUE;
}