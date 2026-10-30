#pragma once
#include <cstdint>
#include <d3d9.h>
#include <d3dx9.h>

#include "ScrollPhysics.h"
#include "ChatBuffer.h"

#include "ScrollbarView.h"
#include "NativeDxutScrollbarView.h"

extern IScrollbarView* g_scrollbar;
extern NativeDxutScrollbarView g_native_scrollbar;

struct SampAddresses {
    uintptr_t chat_info = 0;
    uintptr_t chat_scroll = 0;
    uintptr_t chat_render_entry = 0;
    uintptr_t chat_add_entry = 0;
    uintptr_t chat_recalc_font_size = 0;
    uintptr_t chat_render = 0;
    uintptr_t chat_move_scrollbar = 0;
};

// Глобальное состояние плагина.
extern SampAddresses g_addresses;
extern uintptr_t g_samp_base;
extern bool g_shutdown;
extern bool g_chat_hooks_installed;

// DirectX-объекты.
extern IDirect3DDevice9* g_device;
extern ID3DXFont* g_font;
extern ID3DXSprite* g_sprite;
extern IDirect3DTexture9* g_bg_texture;
extern bool g_graphics_ready;

// Собственный чат.
extern ChatBuffer g_chat_buffer;
extern ScrollPhysics g_scroll;

extern bool g_menu_open;

class FrameClock {
public:
    FrameClock();

    float Tick();

private:
    double frequency_ = 1.0;
    LONGLONG previous_ = 0;
};

extern FrameClock g_frame_clock;

uintptr_t ToAbsolute(uintptr_t relative);
void* GetChatInfo();
void RecalculateScrollBounds();


extern bool g_chat_input_open;

bool IsChatInputOpen();
