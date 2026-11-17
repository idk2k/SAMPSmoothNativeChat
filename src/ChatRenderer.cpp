#include "ChatRenderer.h"
#include "Config.h"
#include "Globals.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

bool CreateGraphics(IDirect3DDevice9* device) {
    if (g_graphics_ready) {
        return true;
    }

    if (!device) {
        return false;
    }

    if (FAILED(D3DXCreateFontW(device, g_config.chat.font_size, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial",
                               &g_font))) {
        Logger::Error("D3DXCreateFontW failed");
        return false;
    }

    if (FAILED(D3DXCreateSprite(device, &g_sprite))) {
        Logger::Error("D3DXCreateSprite failed");

        if (g_font) {
            g_font->Release();
            g_font = nullptr;
        }

        return false;
    }

    if (FAILED(D3DXCreateTexture(device, g_config.chat.width, g_config.chat.height, 1, 0, D3DFMT_A8B8G8R8,
                                 D3DPOOL_MANAGED, &g_bg_texture))) {
        Logger::Error("D3DXCreateTexture failed");

        if (g_sprite) {
            g_sprite->Release();
            g_sprite = nullptr;
        }

        if (g_font) {
            g_font->Release();
            g_font = nullptr;
        }

        return false;
    }

    D3DLOCKED_RECT locked{};

    if (SUCCEEDED(g_bg_texture->LockRect(0, &locked, nullptr, 0))) {
        for (int y = 0; y < g_config.chat.height; ++y) {
            DWORD* row = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
            for (int x = 0; x < g_config.chat.width; ++x) {
                const float t = static_cast<float>(y) / static_cast<float>(g_config.chat.height - 1);
                const BYTE alpha =
                    static_cast<BYTE>(g_config.background.top_alpha +
                                      (g_config.background.bottom_alpha - g_config.background.top_alpha) * t);
                row[x] = (static_cast<DWORD>(alpha) << 24) | 0x00000000;
            }
        }
        g_bg_texture->UnlockRect(0);
    }

    g_graphics_ready = true;
    Logger::Ok("Graphics created");
    return true;
}

void ReleaseGraphics() {
    if (g_bg_texture) {
        g_bg_texture->Release();
        g_bg_texture = nullptr;
    }
    if (g_sprite) {
        g_sprite->Release();
        g_sprite = nullptr;
    }
    if (g_font) {
        g_font->Release();
        g_font = nullptr;
    }
    g_graphics_ready = false;
}

void DrawTextOutlined(const wchar_t* text, RECT rect, D3DCOLOR text_color, int outline_size = 1) {
    const D3DCOLOR outline_color = 0xFF000000;

    RECT outline_rect = rect;

    outline_rect.left += outline_size;
    outline_rect.right += outline_size;
    g_font->DrawTextW(nullptr, text, -1, &outline_rect, DT_LEFT | DT_NOCLIP, outline_color);

    outline_rect = rect;
    outline_rect.left -= outline_size;
    outline_rect.right -= outline_size;
    g_font->DrawTextW(nullptr, text, -1, &outline_rect, DT_LEFT | DT_NOCLIP, outline_color);

    outline_rect = rect;
    outline_rect.top += outline_size;
    outline_rect.bottom += outline_size;
    g_font->DrawTextW(nullptr, text, -1, &outline_rect, DT_LEFT | DT_NOCLIP, outline_color);

    outline_rect = rect;
    outline_rect.top -= outline_size;
    outline_rect.bottom -= outline_size;
    g_font->DrawTextW(nullptr, text, -1, &outline_rect, DT_LEFT | DT_NOCLIP, outline_color);

    g_font->DrawTextW(nullptr, text, -1, &rect, DT_LEFT | DT_NOCLIP, text_color);
}

int MeasureTextWidth(const std::wstring& text, D3DCOLOR color) {
    if (text.empty()) {
        return 0;
    }

    const wchar_t sentinel = L'W';
    std::wstring text_with_sentinel = text + sentinel;

    RECT with_sentinel{};
    g_font->DrawTextW(nullptr, text_with_sentinel.c_str(), -1, &with_sentinel, DT_CALCRECT | DT_NOCLIP, color);

    RECT sentinel_rect{};
    g_font->DrawTextW(nullptr, &sentinel, 1, &sentinel_rect, DT_CALCRECT | DT_NOCLIP, color);

    const int width_with_sentinel = with_sentinel.right - with_sentinel.left;
    const int sentinel_width = sentinel_rect.right - sentinel_rect.left;

    return std::max(0, width_with_sentinel - sentinel_width);
}

void RenderChat(IDirect3DDevice9* device) {
    if (!device || !g_graphics_ready || g_chat_buffer.messages.empty()) {
        return;
    }

    RecalculateScrollBounds();

    if (g_config.chat.line_height <= 0) {
        return;
    }

    const float scroll_px = g_scroll.position;
    const int current_line = static_cast<int>(std::lround(scroll_px / g_config.chat.line_height));
    if (g_scrollbar) {
        g_scrollbar->SetPosition(current_line);
    }

    const int first_index = static_cast<int>(std::floor(scroll_px / g_config.chat.line_height));
    const int last_index = static_cast<int>(std::ceil((scroll_px + g_config.chat.height) / g_config.chat.line_height));

    RECT background_rect;
    background_rect.left = 0;
    background_rect.top = 0;
    background_rect.right = g_config.chat.width;
    background_rect.bottom = g_config.chat.height;

    D3DXVECTOR3 background_position(static_cast<float>(g_config.chat.x), static_cast<float>(g_config.chat.y), 0.0f);

    if (IsChatInputOpen()) {
        g_sprite->Begin(D3DXSPRITE_ALPHABLEND);
        g_sprite->Draw(g_bg_texture, &background_rect, nullptr, &background_position, 0xFFFFFFFF);
        g_sprite->End();
    }

    for (int index = first_index; index <= last_index; ++index) {
        if (index < 0 || index >= static_cast<int>(g_chat_buffer.messages.size())) {
            continue;
        }

        const float y =
            static_cast<float>(g_config.chat.y) + index * static_cast<float>(g_config.chat.line_height) - scroll_px;

        const LONG text_y = static_cast<LONG>(std::lround(y));

        if (y + g_config.chat.line_height <= g_config.chat.y) {
            continue;
        }
        if (y >= g_config.chat.y + g_config.chat.height) {
            continue;
        }

        const ChatMessage& message = g_chat_buffer.messages[index];
        float x = static_cast<float>(g_config.chat.x);

        if (!message.timestamp.empty()) {
            RECT timestamp_rect;
            timestamp_rect.left = static_cast<LONG>(x);
            timestamp_rect.top = text_y;
            timestamp_rect.right = g_config.chat.x + g_config.chat.width;
            timestamp_rect.bottom = text_y + static_cast<LONG>(g_config.chat.line_height);
            DrawTextOutlined(message.timestamp.c_str(), timestamp_rect, g_config.timestamps.color);
            const int timestamp_width = MeasureTextWidth(message.timestamp, g_config.timestamps.color);
            x += static_cast<float>(timestamp_width);
            x += 6.0f;
        }

        for (const TextSegment& segment : message.segments) {
            if (segment.text.empty()) {
                continue;
            }

            RECT text_rect;
            text_rect.left = static_cast<LONG>(x);
            text_rect.top = text_y;
            text_rect.right = g_config.chat.x + g_config.chat.width;
            text_rect.bottom = text_y + static_cast<LONG>(g_config.chat.line_height);

            const int segment_width = MeasureTextWidth(segment.text, segment.color);
            DrawTextOutlined(segment.text.c_str(), text_rect, segment.color);

            x += static_cast<float>(segment_width);
        }
    }
}