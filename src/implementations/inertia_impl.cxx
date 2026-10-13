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

    // Настройки, которые потом можно вывести в ImGui-слайдеры.
    float wheelPower    = 1.35f;
    float response      = 0.045f;
    float stiffness     = 14.0f;
    float damping       = 0.82f;
    float inertia       = 0.94f;
    float reverseBrake  = 0.35f;

private:
    float position = 0.0f;
    float velocity = 0.0f;
    float targetOffset = 0.0f;
    bool active = false;
};

ChatScrollPhysics g_scroll;