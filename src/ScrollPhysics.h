#pragma once
#include <algorithm>
#include <cmath>

class ScrollPhysics {
public:
    float position = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;

    float stiffness = 220.0f;
    float friction = 32.0f;

    float min_position = 0.0f;
    float max_position = 0.0f;

    void SetBounds(float min_pos, float max_pos) {
        min_position = min_pos;
        max_position = max_pos;

        target = std::clamp(target, min_position, max_position);
        position = std::clamp(position, min_position, max_position);
    }

    void AddTarget(float delta_pixels) {
        target = std::clamp(
            target + delta_pixels,
            min_position,
            max_position);
    }

    void CompensateOnAppend(float line_height) {
        if (position > 0.5f) {
            position += line_height;
            target += line_height;

            SetBounds(min_position, max_position);
        }
    }

    void SnapToBottom() {
        target = max_position;
        position = max_position;
        velocity = 0.0f;
    }

    // Мгновенно ставит position в target без пружинного движения.
    // Используется при прямом драге ползунка мышью.
    void SnapToTarget() {
        position = target;
        velocity = 0.0f;
    }

    // Устанавливает цель по доле 0.0..1.0 от всего диапазона.
    // Используется, когда пользователь тянет нативный ползунок мышью.
    void SetTargetFraction(float fraction) {
        fraction = std::clamp(fraction, 0.0f, 1.0f);
        target = min_position + (max_position - min_position) * fraction;
    }

    void Update(float dt) {
        if (dt <= 0.0f) {
            return;
        }

        const float displacement = target - position;
        const float acceleration =
            displacement * stiffness - velocity * friction;

        velocity += acceleration * dt;
        position += velocity * dt;

        position = std::clamp(position, min_position, max_position);

        if (std::fabs(target - position) < 0.15f &&
            std::fabs(velocity) < 4.0f) {
            position = target;
            velocity = 0.0f;
        }
    }
};