#include "Globals.h"
#include "Config.h"
#include "Logger.h"

#include <algorithm>
#include <cmath>

SampAddresses g_addresses;
uintptr_t g_samp_base = 0;
bool g_shutdown = false;
bool g_chat_hooks_installed = false;

IDirect3DDevice9* g_device = nullptr;
ID3DXFont* g_font = nullptr;
ID3DXSprite* g_sprite = nullptr;
IDirect3DTexture9* g_bg_texture = nullptr;
bool g_graphics_ready = false;

ChatBuffer g_chat_buffer;
ScrollPhysics g_scroll;

FrameClock g_frame_clock;

bool g_chat_input_open = false;
bool g_menu_open = false;

FrameClock::FrameClock() {
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    frequency_ = static_cast<double>(frequency.QuadPart);

    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    previous_ = counter.QuadPart;
}

float FrameClock::Tick() {
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);

    const double delta =
        static_cast<double>(counter.QuadPart - previous_) / frequency_;

    previous_ = counter.QuadPart;

    return static_cast<float>((std::min)(delta, 0.05));
}

uintptr_t ToAbsolute(uintptr_t relative) {
    return g_samp_base + relative;
}

void* GetChatInfo() {
    if (!g_addresses.chat_info) {
        return nullptr;
    }

    // 0x26EB80 Ч указатель на CChatInfo*.
    return *reinterpret_cast<void**>(g_addresses.chat_info);
}

void RecalculateScrollBounds() {
    const float total_height =
        static_cast<float>(g_chat_buffer.messages.size()) *
        static_cast<float>(g_config.chat.line_height);

    const float max_offset = (std::max)(
        0.0f,
        total_height - static_cast<float>(g_config.chat.height));

    g_scroll.SetBounds(0.0f, max_offset);

    const int total_lines =
        static_cast<int>(g_chat_buffer.messages.size());

    const int visible_lines =
        g_config.chat.height / g_config.chat.line_height;

    g_scrollbar->SetMetrics(total_lines, visible_lines);
}

bool IsChatInputOpen() {
    // RefInputBox: samp.dll + 0x26EB84 хранит CInput*.
    const uintptr_t input_box_ptr =
        *reinterpret_cast<uintptr_t*>(g_samp_base + 0x26EB84);

    if (!input_box_ptr) {
        return false;
    }

    // CInput::m_bEnabled Ч BOOL по смещению 0x14E0.
    const BOOL enabled = *reinterpret_cast<BOOL*>(
        input_box_ptr + 0x14E0);

    return enabled != FALSE;
}

NullScrollbarView g_null_scrollbar;
NativeDxutScrollbarView g_native_scrollbar;

// ѕока скроллбар не прив€зан Ч используем заглушку.
IScrollbarView* g_scrollbar = &g_null_scrollbar;
