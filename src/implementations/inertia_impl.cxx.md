#define _CRT_SECURE_NO_WARNINGS
#define BUILD_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>
#include <tchar.h>

#include <algorithm>
#include <cstdint>
#include <iostream>

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

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma intrinsic(_ReturnAddress)

bool initialize{false};
bool bMenuOpen{false};
bool bCursorShown{false};

WNDPROC m_pWindowProc;

class ChatScrollPhysics {
public:
    void AddImpulse(int direction, float power) {
        const float impulse = static_cast<float>(direction) * power;

        // Если пользователь меняет направление — сначала гасим прежнюю инерцию.
        if (velocity != 0.0f &&
            std::signbit(velocity) != std::signbit(impulse)) {
            velocity *= reverseBrake;
        }

        targetOffset += impulse;
        velocity += impulse * response;
        active = true;
    }

    void Update(float deltaTime, int minPosition, int maxPosition) {
        if (!active) return;

        const float target = static_cast<float>(targetOffset);
        const float distance = target - position;

        // Разворот в другой направление: активно тормозим старую скорость.
        if (velocity != 0.0f &&
            std::signbit(velocity) != std::signbit(distance)) {
            velocity *= 1.0f - reverseBrake * std::min(1.0f, deltaTime * 60.0f);

            if (std::abs(velocity) < std::abs(distance) * 0.12f) {
                velocity = distance * response * 60.0f;
            }
        }

        // Пружина к целевой позиции.
        velocity += distance * stiffness * deltaTime;

        // Демпфирование: frame-rate independent.
        velocity *= std::pow(damping, deltaTime * 60.0f);

        position += velocity * deltaTime;

        // Мягкие границы.
        if (position < static_cast<float>(minPosition)) {
            position += (static_cast<float>(minPosition) - position) * 0.25f;
            velocity *= 0.55f;
        } else if (position > static_cast<float>(maxPosition)) {
            position += (static_cast<float>(maxPosition) - position) * 0.25f;
            velocity *= 0.55f;
        }

        // Остановка.
        if (std::abs(distance) < 0.05f && std::abs(velocity) < 0.05f) {
            position = target;
            velocity = 0.0f;
            active = false;
        }
    }

    float GetPosition() const { return position; }
    bool IsActive() const { return active; }

    // Settings
    float wheelPower    = 0.85f;
    float response      = 0.085f;
    float stiffness     = 26.0f;
    float damping       = 0.9f;
    float inertia       = 0.77f;
    float reverseBrake  = 0.65f;

private:
    float position = 0.0f;
    float velocity = 0.0f;
    float targetOffset = 0.0f;
    bool active = false;
};

ChatScrollPhysics g_scroll;

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
            static_cast<double>(counter.QuadPart - m_previous) / m_frequency;

        m_previous = counter.QuadPart;

        // Защита от лагов, пауз и переключения окон.
        return static_cast<float>(std::min(delta, 0.05));
    }

private:
    double m_frequency = 1.0;
    LONGLONG m_previous = 0;
};

FrameClock g_frameClock;

using Scroll_t = void(__fastcall*)(void*, void*, int);
Scroll_t fpScroll = nullptr;

auto __fastcall ScrollHooked(void* ecx, void* edx, int nDelta) -> void {
    // В SAMP nDelta обычно равен 1 или -1.
    g_scroll.AddImpulse(nDelta, g_scroll.wheelPower);

    // Всё равно вызываем оригинал: SAMP должен знать о событии.
    fpScroll(ecx, edx, nDelta);
}


void move_scrollbar_pointer(int offset) {
    ((void(__thiscall*)(void* ecx, int delta))(addresses.samp_base + 0x84190))(*(void**)(addresses.g_chat + 0x11E), offset);
}
DWORD get_scrollbar_pointer_pos() {
    return *(DWORD*)((char*)(*(void**)(addresses.g_chat + 0x11E)) + 142);
}

using Render_t = void(__fastcall*)(void*, void*);
Render_t fpRender = nullptr;

auto __fastcall RenderHooked(void* ecx, void* edx) -> void {
    fpRender(ecx, edx);

    const float dt = g_frameClock.Tick();

    const int current = static_cast<int>(get_scrollbar_pointer_pos());
    const int minPosition = static_cast<int>(get_min_scrollbar_position_index());
    const int maxPosition = 87; // Ваш текущий нижний предел.

    g_scroll.Update(dt, minPosition, maxPosition);

    // При активной инерции синхронизируем целый указатель скроллбара.
    if (g_scroll.IsActive()) {
        const int targetInteger =
            static_cast<int>(std::round(g_scroll.GetPosition()));

        int difference = targetInteger - current;

        if (difference != 0) {
            move_scrollbar_pointer(difference);
        }
    }
    std::cout << "dt=" << dt
          << " pos=" << g_scroll.GetPosition()
          << " vel=" << g_scroll.velocity
          << '\n';
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
        // Дробная часть позиции: например, 12.45 -> 0.45.
        const float fractional =
            g_scroll.GetPosition() - std::floor(g_scroll.GetPosition());

        const float rowHeight = 20.0f; // Подставьте реальную высоту строки чата.

        // Направление зависит от того, как SAMP интерпретирует rect.top:
        // если при прокрутке вниз строки должны уезжать вверх — знак минус.
        rect.top += static_cast<long>(fractional * rowHeight);
    }

    fpRenderEntry(ecx, edx, szText, rect, color);
}


using ProtPresent = HRESULT(__stdcall*)(IDirect3DDevice9*, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);
using ProtReset = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using DrawSpriteFP = HRESULT(__stdcall*)(ID3DXSprite*, LPDIRECT3DTEXTURE9, const RECT*, const D3DXVECTOR3*,
                                         const D3DXVECTOR3*, D3DCOLOR);
ProtReset Reset;
ProtPresent Present;
DrawSpriteFP DrawSprite;
HRESULT __stdcall Hooked_Present(IDirect3DDevice9* pDevice, CONST RECT* pSrcRect, CONST RECT* pDestRect,
                                 HWND hDestWindow, CONST RGNDATA* pDirtyRegion);
HRESULT __stdcall Hooked_Reset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentParams);
HRESULT __stdcall Hooked_DrawSprite(ID3DXSprite* sprite, LPDIRECT3DTEXTURE9 pTexture, const RECT* pSrcRect,
                                    const D3DXVECTOR3* pCenter, const D3DXVECTOR3* pPosition, D3DCOLOR Color);

bool ToggleCursor(bool state);
HRESULT __stdcall Hooked_Present(IDirect3DDevice9* pDevice, CONST RECT* pSrcRect, CONST RECT* pDestRect,
                                 HWND hDestWindow, CONST RGNDATA* pDirtyRegion) {
    static bool once = false;
    if (!once) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(**reinterpret_cast<HWND**>(0xC17054));
#pragma warning(push)
#pragma warning(disable : 4996)
        std::string font{getenv("WINDIR")};
        font += "\\Fonts\\Tahoma.TTF";
#pragma warning(pop)
        ImGui::GetIO().Fonts->AddFontFromFileTTF(font.c_str(), 14.0f, NULL,
                                                 ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
        // Theme();
        ImGui_ImplDX9_Init(pDevice);
        once = true;
    }
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (bMenuOpen) {
        if (bCursorShown == 0) {
            bCursorShown = ToggleCursor(1);
        }
        int sx = *reinterpret_cast<int*>(0x00C17044), sy = *reinterpret_cast<int*>(0x00C17048);
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(sx) / 2, static_cast<float>(sy) / 2), ImGuiCond_FirstUseEver);
        ImGui::Begin("SAMPChatEasings Menu", &bMenuOpen,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::SameLine();
        ImGui::SliderFloat("Wheel power", &g_scroll.wheelPower, 0.4f, 3.0f, "%.2f");
        ImGui::SliderFloat("Response", &g_scroll.response, 0.0f, 0.12f, "%.3f");
        ImGui::SliderFloat("Stiffness", &g_scroll.stiffness, 4.0f, 35.0f, "%.1f");
        ImGui::SliderFloat("Damping", &g_scroll.damping, 0.60f, 0.95f, "%.3f");
        ImGui::SliderFloat("Inertia", &g_scroll.inertia, 0.75f, 0.99f, "%.3f");
        ImGui::SliderFloat("Reverse brake", &g_scroll.reverseBrake, 0.05f, 0.90f, "%.2f");

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

        ImGui::End();
    } else if (bCursorShown == 1) {
        bCursorShown = ToggleCursor(0);
    }
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    return Present(pDevice, pSrcRect, pDestRect, hDestWindow, pDirtyRegion);
}

HRESULT __stdcall Hooked_Reset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentParams) {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    return Reset(pDevice, pPresentParams);
}


void InstallSampHooks() {
    utils::MH_CreateAndEnableHook(
        addresses.samp_base + 0x638A0,
        &RenderEntryHooked,
        reinterpret_cast<LPVOID*>(&fpRenderEntry)
    );

    utils::MH_CreateAndEnableHook(
        addresses.samp_base + 0x64010,
        &AddEntryHooked,
        reinterpret_cast<LPVOID*>(&fpAddEntry)
    );

    utils::MH_CreateAndEnableHook(
        addresses.samp_base + 0x63550,
        &RecalcFontSizeHooked,
        reinterpret_cast<LPVOID*>(&fpRecalcFontSize)
    );

    utils::MH_CreateAndEnableHook(
        addresses.samp_base + 0x637F0,
        &ScrollHooked,
        reinterpret_cast<LPVOID*>(&fpScroll)
    );

    utils::MH_CreateAndEnableHook(
        addresses.samp_base + 0x63D70,
        &RenderHooked,
        reinterpret_cast<LPVOID*>(&fpRender)
    );

    MH_ApplyQueued();
}