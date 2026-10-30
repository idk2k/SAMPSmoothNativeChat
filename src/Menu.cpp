#include "Menu.h"
#include "ChatRenderer.h"
#include "Config.h"
#include "Globals.h"
#include "Logger.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui/imgui_impl_win32.h>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool g_imgui_initialized = false;

void menu::Init(IDirect3DDevice9* device) {
    if (g_imgui_initialized || !device) {
        return;
    }

    ImGui::CreateContext();

    // HWND окна игры.
    HWND game_window = **reinterpret_cast<HWND**>(0xC17054);
    ImGui_ImplWin32_Init(game_window);
    ImGui_ImplDX9_Init(device);

    // Шрифт с поддержкой кириллицы.
    char windows_dir[MAX_PATH]{};
    GetEnvironmentVariableA("WINDIR", windows_dir, MAX_PATH);

    std::string font_path = std::string(windows_dir) + "\\Fonts\\Tahoma.TTF";

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

    g_imgui_initialized = true;
    Logger::Ok("ImGui initialized");
}

bool menu::IsInitialized() { return g_imgui_initialized; }

void menu::Render(IDirect3DDevice9* device) {
    if (!g_imgui_initialized || !g_menu_open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("SAMPSmoothNativeChat", &g_menu_open)) {
        // -------------------------------------------------------------
        // Chat
        // -------------------------------------------------------------
        if (ImGui::CollapsingHeader("Chat", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool graphics_changed = false;

            int old_width = g_config.chat.width;
            int old_height = g_config.chat.height;

            ImGui::SliderInt("Y position", &g_config.chat.y, 0, 400);
            ImGui::SliderInt("Width", &g_config.chat.width, 200, 1200);
            ImGui::SliderInt("Height", &g_config.chat.height, 60, 500);

            if (g_config.chat.width != old_width || g_config.chat.height != old_height) {
                graphics_changed = true;
            }

            if (graphics_changed && device) {
                // Пересоздаём фоновую текстуру под новый размер.
                ReleaseGraphics();
                CreateGraphics(device);
                Logger::D3D("Graphics recreated (size changed)");
            }
        }

        // -------------------------------------------------------------
        // Scroll
        // -------------------------------------------------------------
        if (ImGui::CollapsingHeader("Scroll", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Lines per click", &g_config.scroll.lines_per_click, 0.5f, 10.0f, "%.1f");

            ImGui::SliderFloat("Stiffness", &g_config.scroll.stiffness, 50.0f, 600.0f, "%.0f");

            ImGui::SliderFloat("Friction", &g_config.scroll.friction, 5.0f, 80.0f, "%.0f");

            // Синхронизируем физику с конфигом.
            g_scroll.stiffness = g_config.scroll.stiffness;
            g_scroll.friction = g_config.scroll.friction;
        }

        // -------------------------------------------------------------
        // Info
        // -------------------------------------------------------------
        if (ImGui::CollapsingHeader("Info")) {
            ImGui::Text("Messages: %d", (int)g_chat_buffer.messages.size());
            ImGui::Text("Scroll position: %.1f", g_scroll.position);
            ImGui::Text("Scroll target: %.1f", g_scroll.target);
        }
    }
    ImGui::End();
}