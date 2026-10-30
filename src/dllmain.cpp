#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <process.h>
#include <MinHook/MinHook.h>

#include "Globals.h"
#include "Logger.h"
#include "ChatHooks.h"
#include "D3DHooks.h"
#include "ConsoleManager.h"
#include "ChatRenderer.h"
#include "Config.h"

#include <string>

static unsigned int __stdcall InitializationThread(LPVOID) {
    Logger::Info("Initialization thread started");

    g_scroll.stiffness = g_config.scroll.stiffness;
    g_scroll.friction = g_config.scroll.friction;

    int attempt = 0;

    while (!g_shutdown && !g_chat_hooks_installed) {
        ++attempt;

        void* chat = GetChatInfo();

        if (!chat) {
            if (attempt % 50 == 0) {
                Logger::Info(
                    "Waiting for CChatInfo, attempt=" +
                    std::to_string(attempt));
            }

            Sleep(100);
            continue;
        }

        Logger::Info("CChatInfo is ready");

        if (!InstallD3DHooks()) {
            Logger::Error("Failed to install D3D hooks");
            Sleep(1000);
            continue;
        }

        InstallChatHooks();

        g_chat_hooks_installed = true;
        Logger::Info("Initialization finished");
    }

    return 0;
}

static void InitializeAddresses() {
    g_addresses.chat_info = ToAbsolute(0x26EB80);
    g_addresses.chat_scroll = ToAbsolute(0x673C0);
    g_addresses.chat_render_entry = ToAbsolute(0x67470);
    g_addresses.chat_add_entry = ToAbsolute(0x67BE0);
    g_addresses.chat_recalc_font_size = ToAbsolute(0x67120);
    g_addresses.chat_render = ToAbsolute(0x67940);
    g_addresses.chat_move_scrollbar = ToAbsolute(0x887B0);

    Logger::Pointer("CChatInfo*", g_addresses.chat_info);
    Logger::Pointer("CChat::OnScroll", g_addresses.chat_scroll);
    Logger::Pointer("CChat::RenderEntry", g_addresses.chat_render_entry);
    Logger::Pointer("CChat::AddEntry", g_addresses.chat_add_entry);
    Logger::Pointer("CChat::RecalcFontSize", g_addresses.chat_recalc_font_size);
    Logger::Pointer("CChat::Render", g_addresses.chat_render);
    Logger::Pointer("CChat::MoveScrollbar", g_addresses.chat_move_scrollbar);
}

auto WINAPI DllMain(
    HINSTANCE instance,
    DWORD reason,
    LPVOID reserved) -> BOOL
{
    UNREFERENCED_PARAMETER(reserved);

    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(instance);

        if (!ConsoleManager::get_instance().create_console()) {
            return FALSE;
        }

        Logger::Init();

        // ѕуть к config.json р€дом с .asi.
        char module_path[MAX_PATH]{};
        GetModuleFileNameA(instance, module_path, MAX_PATH);

        std::string config_path = module_path;
        size_t last_slash = config_path.find_last_of("\\/");
        if (last_slash != std::string::npos) {
            config_path = config_path.substr(0, last_slash + 1);
        }
        config_path += "config.json";

        LoadConfig(config_path);

        Logger::Info("SAMPChatRebuild loading");

        if (MH_Initialize() != MH_OK) {
            Logger::Error("MH_Initialize failed");
            return FALSE;
        }

        const HMODULE samp_module = GetModuleHandleW(L"samp.dll");

        if (!samp_module) {
            Logger::Error("samp.dll not found");
            return FALSE;
        }

        g_samp_base =
            reinterpret_cast<uintptr_t>(samp_module);

        Logger::Pointer("samp.dll base", g_samp_base);

        InitializeAddresses();

        const uintptr_t thread = _beginthreadex(
            nullptr,
            0,
            &InitializationThread,
            nullptr,
            0,
            nullptr);

        if (thread) {
            CloseHandle(reinterpret_cast<HANDLE>(thread));
        }
        else {
            Logger::Error("Failed to create initialization thread");
        }

        Logger::Ok("SAMPChatRebuild loaded");
        break;
    }

    case DLL_PROCESS_DETACH: {
        Logger::Info("SAMPChatRebuild unloading");

        g_shutdown = true;

        ShutdownChatHooks();
        ShutdownD3DHooks();
        ReleaseGraphics();

        MH_Uninitialize();
        break;
    }

    default:
        break;
    }

    return TRUE;
}