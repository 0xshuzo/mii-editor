#include "ui/wii_cursor.hpp"

#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include <ogc/cache.h>

namespace mii::ui {
namespace {

constexpr int kTextureSize = 128;
constexpr float kBodyHeight = 100.0F;
constexpr float kHandScale = 34.0F / kBodyHeight;
constexpr float kHotspotX = 36.0F;
constexpr float kHotspotY = 17.0F;
constexpr bool kShowHotspotCrosshair = false;

extern "C" {
extern const unsigned char _binary_cursor_hand_rgba_start[];
extern const unsigned char _binary_cursor_mask_rgba_start[];
}

void storeRgba8(unsigned char* destination, const int x, const int y, const unsigned char* source) {
    const int block = (y / 4) * (kTextureSize / 4) + x / 4;
    const int pixel = (y % 4) * 4 + x % 4;
    const int offset = block * 64;
    destination[offset + pixel * 2] = source[3];
    destination[offset + pixel * 2 + 1] = source[0];
    destination[offset + 32 + pixel * 2] = source[1];
    destination[offset + 32 + pixel * 2 + 1] = source[2];
}

}  // namespace

WiiCursor::WiiCursor() {
    initializeTexture(hand_, _binary_cursor_hand_rgba_start);
    initializeTexture(mask_, _binary_cursor_mask_rgba_start);
}

WiiCursor::~WiiCursor() {
    std::free(hand_.pixels);
    std::free(mask_.pixels);
}

void WiiCursor::initializeTexture(Texture& texture, const unsigned char* source) {
    texture.pixels = memalign(32, kTextureSize * kTextureSize * 4);
    std::memset(texture.pixels, 0, kTextureSize * kTextureSize * 4);
    auto* tiled = static_cast<unsigned char*>(texture.pixels);
    for (int y = 0; y < kTextureSize; ++y)
        for (int x = 0; x < kTextureSize; ++x)
            storeRgba8(tiled, x, y, source + (y * kTextureSize + x) * 4);
    DCFlushRange(texture.pixels, kTextureSize * kTextureSize * 4);
    GX_InitTexObj(&texture.object, texture.pixels, kTextureSize, kTextureSize, GX_TF_RGBA8,
                  GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texture.object, GX_LINEAR, GX_LINEAR, 0.0F, 0.0F, 0.0F,
                     GX_FALSE, GX_FALSE, GX_ANISO_1);
}

void WiiCursor::update(const bool pointerValid, const Point pointer) {
    if (!pointerValid) {
        visible_ = false;
        return;
    }
    visible_ = true;
    pointer_ = pointer;
}

void WiiCursor::render(UiRenderer& renderer) const {
    if (!visible_) return;
    const Rect hand{pointer_.x - kHotspotX * kHandScale, pointer_.y - kHotspotY * kHandScale,
                    kTextureSize * kHandScale, kTextureSize * kHandScale};
    const auto expanded = [&hand](const float factor) {
        const float width = hand.width * factor;
        const float height = hand.height * factor;
        return Rect{hand.x - (width - hand.width) / 2.0F, hand.y - (height - hand.height) / 2.0F, width, height};
    };
    renderer.clearClip();
    renderer.drawTexture(mask_.object, expanded(1.08F), {14, 31, 58, 55});
    renderer.drawTexture(hand_.object, hand);
    if (kShowHotspotCrosshair) {
        renderer.useSolidPrimitives();
        renderer.fillRectangle({pointer_.x - 3.0F, pointer_.y, 6.0F, 1.0F}, {255, 0, 0, 255});
        renderer.fillRectangle({pointer_.x, pointer_.y - 3.0F, 1.0F, 6.0F}, {255, 0, 0, 255});
    }
}

}  // namespace mii::ui
