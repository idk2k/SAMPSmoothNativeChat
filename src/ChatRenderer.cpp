#include "ChatRenderer.h"
#include "Globals.h"
#include "Config.h"
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

    if (FAILED(D3DXCreateFontW(
        device,
        g_config.chat.font_size,   // было config::kLineHeight
        0,
        FW_NORMAL,
        1,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Arial",
        &g_font)))
    {
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

    if (FAILED(D3DXCreateTexture(
        device,
        g_config.chat.width,
        g_config.chat.height,
        1,
        0,
        D3DFMT_A8B8G8R8,
        D3DPOOL_MANAGED,
        &g_bg_texture)))
    {
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
            DWORD* row = reinterpret_cast<DWORD*>(
                reinterpret_cast<BYTE*>(locked.pBits) +
                y * locked.Pitch);

            for (int x = 0; x < g_config.chat.width; ++x) {
                // Вертикальный градиент: сверху почти прозрачно,
                // снизу плотнее.
                const float t = static_cast<float>(y) /
                    static_cast<float>(g_config.chat.height - 1);

                const BYTE alpha = static_cast<BYTE>(
                    g_config.background.top_alpha +
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

void RenderChat(IDirect3DDevice9* device) {
    if (!device || !g_graphics_ready || g_chat_buffer.messages.empty()) {
        return;
    }

    RecalculateScrollBounds();

    const float scroll_px = g_scroll.position;

    const int current_line = static_cast<int>(
        std::lround(scroll_px / g_config.chat.line_height));

    g_scrollbar->SetPosition(current_line);

    const int first_index = static_cast<int>(
        std::floor(scroll_px / g_config.chat.line_height));

    const int last_index = static_cast<int>(
        std::ceil(
            (scroll_px + g_config.chat.height) /
            g_config.chat.line_height));

    // -------------------------------------------------------------------------
    // Фон чата.
    // Рисуется нашим ID3DXSprite.
    // -------------------------------------------------------------------------

    RECT background_rect;
    background_rect.left = 0;
    background_rect.top = 0;
    background_rect.right = g_config.chat.width;
    background_rect.bottom = g_config.chat.height;

    D3DXVECTOR3 background_position(
        static_cast<float>(g_config.chat.x),
        static_cast<float>(g_config.chat.y),
        0.0f);

    // Фон рисуем только при открытом вводе.
    if (g_chat_input_open) {
        g_sprite->Begin(D3DXSPRITE_ALPHABLEND);

        g_sprite->Draw(
            g_bg_texture,
            &background_rect,
            nullptr,
            &background_position,
            0xFFFFFFFF);

        g_sprite->End();
    }

    // -------------------------------------------------------------------------
    // Текст чата.
    // ВАЖНО: рисуем через внутренний спрайт ID3DXFont (sprite = nullptr).
    // Наш g_sprite для текста не используем.
    // -------------------------------------------------------------------------

    for (int index = first_index; index <= last_index; ++index) {
        if (index < 0 ||
            index >= static_cast<int>(g_chat_buffer.messages.size())) {
            continue;
        }

        const float y =
            static_cast<float>(g_config.chat.y) +
            index * static_cast<float>(g_config.chat.line_height) -
            scroll_px;

        // Строка полностью ушла за верхнюю границу.
        if (y + g_config.chat.line_height <= g_config.chat.y) {
            continue;
        }

        // Строка полностью ушла за нижнюю границу.
        if (y >= g_config.chat.y + g_config.chat.height) {
            continue;
        }

        const ChatMessage& message =
            g_chat_buffer.messages[index];

        float x = static_cast<float>(g_config.chat.x);

        // Рисуем таймстамп [HH:MM], если он есть.
        if (!message.timestamp.empty()) {
            RECT timestamp_rect;
            timestamp_rect.left = static_cast<LONG>(x);
            timestamp_rect.top = static_cast<LONG>(y);
            timestamp_rect.right = g_config.chat.x + g_config.chat.width;
            timestamp_rect.bottom = static_cast<LONG>(
                y + g_config.chat.line_height);

            g_font->DrawTextW(
                nullptr,
                message.timestamp.c_str(),
                -1,
                &timestamp_rect,
                DT_LEFT | DT_NOCLIP,
                g_config.timestamps.color);

            // Сдвигаем x на ширину таймстампа + небольшой отступ.
            RECT measured_timestamp{};
            g_font->DrawTextW(
                nullptr,
                message.timestamp.c_str(),
                -1,
                &measured_timestamp,
                DT_CALCRECT | DT_NOCLIP,
                g_config.timestamps.color);

            x += static_cast<float>(
                measured_timestamp.right - measured_timestamp.left);

            x += 6.0f; // отступ между временем и текстом
        }


        for (const TextSegment& segment : message.segments) {
            if (segment.text.empty()) {
                continue;
            }

            RECT text_rect;
            text_rect.left = static_cast<LONG>(x);
            text_rect.top = static_cast<LONG>(y);
            text_rect.right = g_config.chat.x + g_config.chat.width;
            text_rect.bottom = static_cast<LONG>(
                y + g_config.chat.line_height);

            // Сначала измеряем ширину сегмента.
            RECT measured{};
            g_font->DrawTextW(
                nullptr,
                segment.text.c_str(),
                -1,
                &measured,
                DT_CALCRECT | DT_NOCLIP,
                segment.color);

            const int segment_width =
                measured.right - measured.left;

            // Затем рисуем сегмент.
            const int drawn = g_font->DrawTextW(
                nullptr,
                segment.text.c_str(),
                -1,
                &text_rect,
                DT_LEFT | DT_NOCLIP,
                segment.color);


            x += static_cast<float>(segment_width);
        }
    }
}