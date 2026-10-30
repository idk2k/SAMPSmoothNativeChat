#include "ChatHooks.h"
#include "Config.h"
#include "Globals.h"
#include "Logger.h"
#include "NativeDxutScrollbarView.h"
#include <MinHook/MinHook.h>
#include <Windows.h>
#include <iomanip>
#include <sstream>
#include <string>

static bool g_scrollbar_bound = false;

// Привязка нативного DXUT-скроллбара.
// Вызывается один раз — при первом сообщении в чате.
static void EnsureScrollbarBound() {
    if (g_scrollbar_bound) {
        return;
    }

    void* chat = GetChatInfo();

    if (!chat) {
        return;
    }

    // CChat::m_pScrollbar — смещение 0x11E в этой сборке R5.
    void* scrollbar = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(chat) + 0x11E);

    if (!scrollbar) {
        return;
    }

    g_native_scrollbar.SetScrollbar(scrollbar);

    // CDXUTScrollBar::Scroll — samp.dll + 0x887B0.
    g_native_scrollbar.SetScrollFunction(g_samp_base + 0x887B0);

    // Драг ползунка мышью -> чат следует за ним 1:1.
    g_native_scrollbar.on_external_scroll = [](float fraction) {
        g_scroll.SetTargetFraction(fraction);
        g_scroll.SnapToTarget();
    };

    // Калибровка диапазона нативного скроллбара.
    g_native_scrollbar.Calibrate();

    // Переключаем активную вьюшку с заглушки на нативную.
    g_scrollbar = &g_native_scrollbar;

    g_scrollbar_bound = true;

    Logger::Ok("Native DXUT scrollbar bound and calibrated");
}

// ---------------------------------------------------------------------------
// Адреса SA-MP 0.3.7 R5.
// Все адреса относительные: final = samp.dll base + offset.
// ---------------------------------------------------------------------------

namespace offsets {
constexpr uintptr_t kChatInfo = 0x26EB80;
constexpr uintptr_t kChatScroll = 0x673C0;
constexpr uintptr_t kChatRenderEntry = 0x67470;
constexpr uintptr_t kChatAddEntry = 0x67BE0;
constexpr uintptr_t kChatRecalcFontSize = 0x67120;
constexpr uintptr_t kChatRender = 0x67940;
constexpr uintptr_t kChatMoveScrollbar = 0x887B0;
} // namespace offsets

// ---------------------------------------------------------------------------
// CChat::AddEntry
// void CChat::AddEntry(int, const char*, const char*, D3DCOLOR, D3DCOLOR)
// thiscall: ecx = this, edx = пустой регистр.
// ---------------------------------------------------------------------------

using AddEntry_t = void(__fastcall*)(void* ecx, void* edx, int type, const char* text, const char* prefix,
                                     D3DCOLOR text_color, D3DCOLOR prefix_color);

static AddEntry_t fp_add_entry = nullptr;

static std::wstring ToWideCp1251(const char* text) {
    if (!text || !*text) {
        return L"";
    }

    const int size = MultiByteToWideChar(1251, 0, text, -1, nullptr, 0);

    if (size <= 0) {
        return L"";
    }

    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(1251, 0, text, -1, &wide[0], size);

    // Убираем завершающий '\0', который MultiByteToWideChar
    // включает в возвращаемый размер при cbMultiByte = -1.
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }

    return wide;
}

// Конвертация UTF-16 -> UTF-8 для консольного лога.
static std::string ToUtf8Log(const std::wstring& wide) {
    if (wide.empty()) {
        return "";
    }

    const int size =
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);

    if (size <= 0) {
        return "";
    }

    std::string utf8(static_cast<size_t>(size), '\0');

    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), &utf8[0], size, nullptr, nullptr);

    return utf8;
}

static void __fastcall AddEntryHooked(void* ecx, void* edx, int type, const char* text, const char* prefix,
                                      D3DCOLOR text_color, D3DCOLOR prefix_color) {
    // Нативный буфер продолжаем наполнять.
    if (fp_add_entry) {
        fp_add_entry(ecx, edx, type, text, prefix, text_color, prefix_color);
    }

    std::wstring wide_text = ToWideCp1251(text);
    std::wstring wide_prefix = ToWideCp1251(prefix);

    std::wstring full;

    if (!wide_prefix.empty()) {
        full = wide_prefix + L" " + wide_text;
    } else {
        full = wide_text;
    }

    if (full.empty()) {
        return;
    }

    if ((text_color & 0xFF000000) == 0) {
        text_color |= 0xFF000000;
    }

    // Запоминаем, был ли пользователь внизу истории ДО добавления сообщения.
    const bool was_at_bottom = g_scroll.position >= g_scroll.max_position - 0.5f;

    // Текущее время для таймстампа.
    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t timestamp[16]{};
    swprintf_s(timestamp, L"[%02d:%02d]", st.wHour, st.wMinute);

    g_chat_buffer.Append(full, text_color, timestamp);

    {
        std::ostringstream color_stream;
        color_stream << std::hex << std::uppercase << text_color;

        Logger::SAMP("AddEntry: type=" + std::to_string(type) + ", color=0x" + color_stream.str() + ", prefix='" +
                     ToUtf8Log(wide_prefix) + "'" + ", text='" + ToUtf8Log(wide_text) + "'");
    }

    // Привязываем скроллбар при первом сообщении,
    // когда он уже точно нужен и видим.
    if (g_chat_buffer.messages.size() > 10) {
        EnsureScrollbarBound();
    }

    RecalculateScrollBounds();

    if (was_at_bottom) {
        // Пользователь был внизу — остаёмся внизу,
        // новое сообщение сразу видно.
        g_scroll.SnapToBottom();
    }
}

// ---------------------------------------------------------------------------
// CChat::Render
// Полностью отключаем нативный рендер чата.
// ---------------------------------------------------------------------------

using ChatRender_t = void(__fastcall*)(void* ecx, void* edx);

static ChatRender_t fp_chat_render = nullptr;

static void __fastcall ChatRenderHooked(void* ecx, void* edx) {
    // Оригинал не вызывается.
    // Нативный чат не рисуется: ни фон, ни строки, ни скроллбар.
}

// ---------------------------------------------------------------------------
// CChat::OnScroll
// ---------------------------------------------------------------------------

using ChatScroll_t = void(__fastcall*)(void* ecx, void* edx, int delta);

static ChatScroll_t fp_chat_scroll = nullptr;

static void __fastcall ChatScrollHooked(void* ecx, void* edx, int delta) {
    if (delta == 0) {
        return;
    }

    // Колесо вверх: delta > 0 — показываем более старые сообщения.
    const float direction = (delta > 0) ? -1.0f : 1.0f;

    g_scroll.AddTarget(direction * g_config.scroll.lines_per_click * static_cast<float>(g_config.chat.line_height));

    // Нативную прокрутку не вызываем.
    // Если нужно временно вернуть нативный скроллбар:
    // fp_chat_scroll(ecx, edx, delta);
}

// ---------------------------------------------------------------------------
// Установка хуков.
// ---------------------------------------------------------------------------

static bool CreateAndEnableHook(const char* name, uintptr_t target, void* detour, void** original) {
    Logger::Hook(std::string("Target: ") + name);
    Logger::Pointer(name, target);

    MH_STATUS status = MH_CreateHook(reinterpret_cast<LPVOID>(target), detour, original);

    if (status != MH_OK) {
        Logger::Error(std::string(name) + ": MH_CreateHook failed, status=" + std::to_string(static_cast<int>(status)));
        return false;
    }

    status = MH_EnableHook(reinterpret_cast<LPVOID>(target));

    if (status != MH_OK) {
        Logger::Error(std::string(name) + ": MH_EnableHook failed, status=" + std::to_string(static_cast<int>(status)));
        return false;
    }

    Logger::Ok(std::string(name) + " installed");
    return true;
}

void InstallChatHooks() {
    Logger::Hook("Installing SA-MP chat hooks");

    CreateAndEnableHook("CChat::AddEntry", ToAbsolute(offsets::kChatAddEntry), &AddEntryHooked,
                        reinterpret_cast<void**>(&fp_add_entry));

    CreateAndEnableHook("CChat::Render", ToAbsolute(offsets::kChatRender), &ChatRenderHooked,
                        reinterpret_cast<void**>(&fp_chat_render));

    CreateAndEnableHook("CChat::OnScroll", ToAbsolute(offsets::kChatScroll), &ChatScrollHooked,
                        reinterpret_cast<void**>(&fp_chat_scroll));

    Logger::Info("SA-MP chat hooks installed");
}

void ShutdownChatHooks() {
    MH_DisableHook(reinterpret_cast<LPVOID>(ToAbsolute(offsets::kChatAddEntry)));

    MH_DisableHook(reinterpret_cast<LPVOID>(ToAbsolute(offsets::kChatRender)));

    MH_DisableHook(reinterpret_cast<LPVOID>(ToAbsolute(offsets::kChatScroll)));

    MH_RemoveHook(reinterpret_cast<LPVOID>(ToAbsolute(offsets::kChatAddEntry)));

    MH_RemoveHook(reinterpret_cast<LPVOID>(ToAbsolute(offsets::kChatRender)));

    MH_RemoveHook(reinterpret_cast<LPVOID>(ToAbsolute(offsets::kChatScroll)));
}