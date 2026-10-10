#pragma once

#include <cstdint>

namespace mii::ui {

constexpr float kDesignWidth = 854.0F;
constexpr float kDesignHeight = 480.0F;

struct Color {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

struct Point {
    float x;
    float y;
};

struct Rect {
    float x;
    float y;
    float width;
    float height;

    [[nodiscard]] constexpr float right() const { return x + width; }
    [[nodiscard]] constexpr float bottom() const { return y + height; }

    [[nodiscard]] bool contains(const Point point) const {
        return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
    }
};

// Pixel-space destination for the shared logical design canvas.
struct Viewport {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

}  // namespace mii::ui
