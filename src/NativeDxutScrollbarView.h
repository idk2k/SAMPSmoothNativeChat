#pragma once
#include "ScrollbarView.h"
#include <Windows.h>
#include <cstdint>
#include <cmath>
#include <functional>

class NativeDxutScrollbarView : public IScrollbarView {
public:
    using Scroll_t = void(__thiscall*)(void* scrollbar, int delta);

    std::function<void(float)> on_external_scroll;

    void SetScrollbar(void* scrollbar) {
        scrollbar_ = scrollbar;
    }

    void SetScrollFunction(uintptr_t function_address) {
        scroll_ = reinterpret_cast<Scroll_t>(function_address);
    }

    void SetMetrics(int total_lines, int visible_lines) override {
        total_lines_ = total_lines;
        visible_lines_ = visible_lines;
        max_line_ = (std::max)(0, total_lines_ - visible_lines_);
    }

    void SetPosition(int current_line) override {
        if (!scrollbar_ || !scroll_) {
            return;
        }

        const ULONGLONG now = GetTickCount64();
        const int native_pos = ReadNativePosition();

        // -------------------------------------------------------------
        // Позиция изменилась.
        // -------------------------------------------------------------
        if (native_pos != last_native_pos_) {
            if (native_pos == expected_native_pos_) {
                // Это движение от нашей же команды Scroll —
                // НЕ внешнего драга. Ничего не делаем.
            }
            else if (native_max_ > 0 && on_external_scroll) {
                // Реальный драг ползунка мышью.
                external_until_ = now + 250;

                const float fraction =
                    static_cast<float>(native_pos) /
                    static_cast<float>(native_max_);

                on_external_scroll(fraction);
            }
        }
        // -------------------------------------------------------------
        // Корректирующая синхронизация от физики.
        // -------------------------------------------------------------
        else if (max_line_ > 0 &&
            native_max_ > 0 &&
            now >= external_until_)
        {
            const float fraction =
                static_cast<float>(current_line) /
                static_cast<float>(max_line_);

            const int desired = static_cast<int>(
                std::lround(fraction * native_max_));

            const int delta = desired - native_pos;

            if (delta != 0) {
                scroll_(scrollbar_, delta);

                // Запоминаем, куда должна приехать позиция после
                // нашей команды, чтобы не принять это за драг мышью.
                expected_native_pos_ = desired;
            }
        }

        last_native_pos_ = ReadNativePosition();
    }

    void Calibrate() {
        if (!scrollbar_ || !scroll_) {
            return;
        }

        scroll_(scrollbar_, 100000);
        native_max_ = ReadNativePosition();

        scroll_(scrollbar_, -100000);
        last_native_pos_ = ReadNativePosition();
    }

private:
    int ReadNativePosition() const {
        if (!scrollbar_) {
            return 0;
        }

        return *reinterpret_cast<int*>(
            reinterpret_cast<uintptr_t>(scrollbar_) + 0x8E);
    }

    void* scrollbar_ = nullptr;
    Scroll_t scroll_ = nullptr;

    int total_lines_ = 0;
    int visible_lines_ = 0;
    int max_line_ = 0;

    int native_max_ = 0;
    int last_native_pos_ = 0;

    // Ожидаемая позиция после нашей собственной Scroll-команды.
    int expected_native_pos_ = -1;

    // До этого времени физика не корректирует ползунок.
    ULONGLONG external_until_ = 0;
};