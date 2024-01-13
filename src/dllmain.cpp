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

DWORD SAMP_GAME_PTR = 0x21A10C;
DWORD SAMP_FUNC_ADDCLIENTCMD = 0x65AD0;
DWORD SAMP_FUNC_TOGGLECURSOR = 0x9BD30;
DWORD SAMP_FUNC_CURSORUNLOCKACTORCAM = 0x9BC10;

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
        if (ImGui::Button("some sfuffffsdjfdsijfsi")) {
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

struct Addresses {
    std::uintptr_t samp_base;
    std::uintptr_t g_chat;
    std::uintptr_t rakpeer_connect;
} addresses{};

HRESULT __stdcall Hooked_DrawSprite(ID3DXSprite* sprite, LPDIRECT3DTEXTURE9 pTexture, const RECT* pSrcRect,
                                    const D3DXVECTOR3* pCenter, const D3DXVECTOR3* pPosition, D3DCOLOR Color) {
    static RECT rectsrc{};
    static bool once{};
    static D3DXVECTOR3 rect_test{};

    if (_ReturnAddress() == (void*)(addresses.samp_base + 0x642C9)) {
        if (!once) {
            rectsrc.right = 0x00FFFFFF;
            rectsrc.bottom = 0x10D;
            rectsrc.top = 0x1E;

            rect_test.x = 0;
            rect_test.y = 10;
            rect_test.z = 0;

            once = true;
        }
        return DrawSprite(sprite, pTexture, &rectsrc, pCenter, &rect_test, Color);
    }
    return DrawSprite(sprite, pTexture, pSrcRect, pCenter, pPosition, Color);
}

void InstallD3DHook() {
    DWORD pDevice = *reinterpret_cast<DWORD*>(0xC97C28);
    void** vTable = *reinterpret_cast<void***>(pDevice);
    void** vTable2 = **reinterpret_cast<void****>(addresses.g_chat + 0x63AA);

    MH_CreateHook(vTable[17], Hooked_Present, reinterpret_cast<LPVOID*>(&Present));
    MH_EnableHook(vTable[17]);
    MH_CreateHook(vTable[16], Hooked_Reset, reinterpret_cast<LPVOID*>(&Reset));
    MH_EnableHook(vTable[16]);
    MH_CreateHook(vTable2[9], Hooked_DrawSprite, reinterpret_cast<LPVOID*>(&DrawSprite));
    MH_EnableHook(vTable2[9]);
}

bool ToggleCursor(bool state) {
    void* obj = *(void**)(addresses.samp_base + SAMP_GAME_PTR);
    ((void(__thiscall*)(void*, int, bool))(addresses.samp_base + SAMP_FUNC_TOGGLECURSOR))(obj, state ? 3 : 0, !state);
    if (!state) ((void(__thiscall*)(void*))(addresses.samp_base + SAMP_FUNC_CURSORUNLOCKACTORCAM))(obj);
    return state;
}

struct Parameters {
    bool bNewEntryAdded;
    bool Scroll;
    bool ScrollToTop;
    bool ScrollToBottom;
    bool running;
    int max_adjust_value;
    int increments;
    double prev_easing;
    int message_top_offset;
} global_params{false, false, false, false, false, 25, 0, .0f, 0};

int scroll_for_reach = 0;
bool temp_holder{false};

ULONGLONG delayForScroll = 999999999;
ULONGLONG prevTimeScroll = 0;

using Scroll_t = void(__fastcall*)(void*, void*, int);
Scroll_t fpScroll{};
auto __fastcall ScrollHooked(void* reg_ecx, void* reg_edx, int nDelta) -> void {
    global_params.Scroll = true;
    // Check for abruptly change the direction of scrolling
    if (global_params.ScrollToTop && nDelta == -1) {
        scroll_for_reach = 0;
    }
    if (global_params.ScrollToBottom && nDelta == 1) {
        scroll_for_reach = 0;
    }
    // Setting right scroll directions
    if (nDelta == 1) {
        global_params.ScrollToTop = true;
        global_params.ScrollToBottom = false;
    } else {
        global_params.ScrollToBottom = true;
        global_params.ScrollToTop = false;
    }
    scroll_for_reach += 1;
    // Minimum, and good delay for scrolling delta of time.
    delayForScroll = (ULONGLONG)10;
}

using RecalcFontSize_t = void(__fastcall*)(void*, void*);
RecalcFontSize_t fpRecalcFontSize{};
auto __fastcall RecalcFontSizeHooked(void* reg_ecx, void* reg_edx) -> void { fpRecalcFontSize(reg_ecx, reg_edx); }

DWORD& get_min_scrollbar_position_index() { return *(DWORD*)((char*)(*(void**)(addresses.g_chat + 0x11E)) + 150); }

using AddEntry_t = void(__fastcall*)(void*, void*, int, const char*, const char*, structures::D3DCOLOR,
                                     structures::D3DCOLOR);
AddEntry_t fpAddEntry{};
auto __fastcall AddEntryHooked(void* reg_ecx, void* reg_edx, int nType, const char* szText, const char* szPrefix,
                               structures::D3DCOLOR textColor, structures::D3DCOLOR prefixColor) -> void {
    global_params.bNewEntryAdded = true;

    // This is counter of new added messages. Purpose of it - just save real minimum index.
    // This min value uses for scrollbar size on left side from chat lines.
    //
    // Start value is 101-8, just coz AddEntry hooked at RakNetConnect, and 7 messages added before it (samp connect
    // msgs, CLEO plugin).
    static int start_min = 101 - 8;
    if (!addresses.g_chat) {
        utils::protect_safe_memory_copy(&addresses.g_chat, (void*)(addresses.samp_base + 0x21A0E4), 0x4);
    }
    start_min -= 1;
    // Min value is page size. [TODO]: get page size directly from memory (remove hard-code value).
    get_min_scrollbar_position_index() = start_min >= 1 ? std::min(0x57, start_min) : 1;

    fpAddEntry(reg_ecx, reg_edx, nType, szText, szPrefix, textColor, prefixColor);
}

// ===========
bool scroll_process = false;
int scroll_increments = 0;
// ===========

ULONGLONG delay = 12;
ULONGLONG prevTimeAddMessage = 0;
ULONGLONG startTimer = 0;

bool force_return_flag = false;

using Render_t = void(__fastcall*)(void*, void*);
Render_t fpRender{};
auto __fastcall Render(void* reg_ecx, void* reg_edx) -> void {
    if (force_return_flag) {
        force_return_flag = false;
    }
    fpRender(reg_ecx, reg_edx);
    if (global_params.running) {
        if (!prevTimeAddMessage) prevTimeAddMessage = GetTickCount64();
        if (GetTickCount64() - prevTimeAddMessage >= delay) {
            prevTimeAddMessage = GetTickCount64();
            global_params.increments += 1;
        }
    }
    if (scroll_process) {
        if (!prevTimeScroll) prevTimeScroll = GetTickCount64();
        if (GetTickCount64() - prevTimeScroll >= delayForScroll) {
            prevTimeScroll = GetTickCount64();
            // Some calculations for smooth scroll, depending on number of scroll_for_reach. Its parabola
            // if scroll_for_reach < 3, and just line if >=3;
            scroll_increments += [&]() -> int {
                if (scroll_for_reach >= 3) {
                    return 21;
                } else {
                    return (53 / 24) * scroll_for_reach * scroll_for_reach + 1;
                }
            }();
        }
    }
}

void move_scrollbar_pointer(int offset) {
    ((void(__thiscall*)(void* ecx, int delta))(addresses.samp_base + 0x84190))(*(void**)(addresses.g_chat + 0x11E),
                                                                               offset);
}

DWORD get_scrollbar_pointer_pos() { return *(DWORD*)((char*)(*(void**)(addresses.g_chat + 0x11E)) + 142); }

using RenderEntry_t = void(__fastcall*)(void*, void*, const char*, structures::CRect, structures::D3DCOLOR);
RenderEntry_t fpRenderEntry{};
auto __fastcall RenderEntryHooked(void* reg_ecx, void* reg_edx, const char* szText, structures::CRect rect,
                                  structures::D3DCOLOR color) -> void {
    // 'New Message Added' Behavior Managing
    if (global_params.bNewEntryAdded) {
        global_params.bNewEntryAdded = false;
        if (!global_params.running) {
            global_params.running = true;
            global_params.increments = 0;
        }
    }
    if (global_params.increments > 20) {
        global_params.running = false;
    }
    if (global_params.running) {
        rect.top += 20 - (long)(global_params.increments);
    }
    // 'Scroll by Mouse' Behavior Managing
    if (scroll_for_reach) {
        if (!scroll_process) {
            scroll_process = true;
            scroll_increments = 0;
            scroll_for_reach -= 1;
            startTimer = GetTickCount64();
            if (global_params.ScrollToBottom) {
                force_return_flag = true;
                temp_holder = true;
                if (get_scrollbar_pointer_pos() == 87) {
                    scroll_process = false;
                    scroll_for_reach = 0;
                }
            }
            if (global_params.ScrollToTop) {
                // This is check for min scroll position, which can change if new message added.
                if (get_scrollbar_pointer_pos() == get_min_scrollbar_position_index()) {
                    scroll_process = false;
                    scroll_for_reach = 0;
                }
            }
        }
    }
    if (scroll_increments > 20) {
        scroll_process = false;
        scroll_increments = 0;
        if (global_params.ScrollToTop) {
            move_scrollbar_pointer(-1);
            force_return_flag = true;
        }
    }
    if (global_params.ScrollToTop) {
        if (force_return_flag) rect.top += 20;
    }
    if (scroll_process) {
        if (global_params.ScrollToTop) rect.top += (long)(scroll_increments);
        if (global_params.ScrollToBottom) {
            if (temp_holder) {
                move_scrollbar_pointer(1);
                temp_holder = false;
            }
            if (!force_return_flag) {
                rect.top -= (long)(scroll_increments)-20;
            }
        }
    }
    fpRenderEntry(reg_ecx, reg_edx, szText, rect, color);
}

LRESULT __stdcall WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_F12: {
                    bMenuOpen = !bMenuOpen;
                    break;
                }
            }
            break;
        }
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
    return CallWindowProcA(m_pWindowProc, hWnd, msg, wParam, lParam);
}

using RakPeer_Connect = bool(__fastcall*)(void*, void*, const char*, unsigned short, char*, int);
RakPeer_Connect fpConnect{};
auto __fastcall RakPeerConnectHooked(void* reg_ecx, void* reg_edx, const char* host, unsigned short port,
                                     char* passwordData, int passwordDataLength) -> bool {
    if (!initialize) {
        m_pWindowProc = reinterpret_cast<WNDPROC>(
            SetWindowLongA(*reinterpret_cast<HWND*>(0xC97C1CU), GWL_WNDPROC, reinterpret_cast<LONG>(WndProcHandler)));
        if (!addresses.g_chat) {
            utils::protect_safe_memory_copy(&addresses.g_chat, (void*)(addresses.samp_base + 0x21A0E4), 0x4);
        }

        // Start value of min index, used for scrollbar size on left side from chat lines.
        get_min_scrollbar_position_index() = 0x57;

        InstallD3DHook();

        utils::protect_safe_memory_set((void*)(addresses.samp_base + 0x6441C + 6U), 1, 1);

        utils::MH_CreateAndEnableHook(static_cast<std::uintptr_t>(addresses.samp_base + 0x638A0), &RenderEntryHooked,
                                      reinterpret_cast<LPVOID*>(&fpRenderEntry));
        utils::MH_CreateAndEnableHook(static_cast<std::uintptr_t>(addresses.samp_base + 0x64010), &AddEntryHooked,
                                      reinterpret_cast<LPVOID*>(&fpAddEntry));
        utils::MH_CreateAndEnableHook(static_cast<std::uintptr_t>(addresses.samp_base + 0x63550), &RecalcFontSizeHooked,
                                      reinterpret_cast<LPVOID*>(&fpRecalcFontSize));

        utils::MH_CreateAndEnableHook(static_cast<std::uintptr_t>(addresses.samp_base + 0x637F0), &ScrollHooked,
                                      reinterpret_cast<LPVOID*>(&fpScroll));

        utils::MH_CreateAndEnableHook(static_cast<std::uintptr_t>(addresses.samp_base + 0x63D70), &Render,
                                      reinterpret_cast<LPVOID*>(&fpRender));

        initialize = true;
    }
    return fpConnect(reg_ecx, reg_edx, host, port, passwordData, passwordDataLength);
}

auto WINAPI DllMain(HINSTANCE dllinstance, DWORD reason, LPVOID lpvReserved) -> BOOL {
    UNREFERENCED_PARAMETER(lpvReserved);
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(dllinstance);
            if (ConsoleManager::get_instance().create_console() == FALSE) {
                std::cout << "[ERROR]: ConsoleManager::get_instance().create_console() failed" << std::endl;
                return FALSE;
            }
            if (MH_STATUS mh_init_status = MH_Initialize(); mh_init_status != MH_OK) {
                std::cout << "[ERROR]: MH_Initialize() failed, return code: " << mh_init_status << std::endl;
                return FALSE;
            }
            auto samp_handle = GetModuleHandleW(L"samp.dll");
            if (!samp_handle) {
                std::cout << "[ERROR]: samp_handle getting failed" << std::endl;
                return FALSE;
            }
            addresses.samp_base = reinterpret_cast<std::uintptr_t>(samp_handle);
            addresses.rakpeer_connect = 0x3ABB0;
            if (!addresses.rakpeer_connect) {
                std::cout << "[ERROR]: no RakPeer__Connect address" << std::endl;
                return FALSE;
            }
            utils::MH_CreateAndEnableHook(static_cast<std::uintptr_t>(addresses.samp_base + addresses.rakpeer_connect),
                                          &RakPeerConnectHooked, reinterpret_cast<LPVOID*>(&fpConnect));
            break;
        }
    }
    return TRUE;
}