#pragma once

#include <array>
#include <string_view>

#include <gccore.h>

#include "ui/ui_types.hpp"

namespace mii::ui {

enum class MkwSymbol {
    Star,
    WiimoteTwo,
    WiimoteA,
    WiimoteB,
    WiimotePlus,
    Coin,
    Balloon,
    Trophy,
};

// Small immediate-mode renderer for the app's native Wii UI. It deliberately has
// no knowledge of Mii data or input handling.
class UiRenderer {
public:
    UiRenderer();
    ~UiRenderer();

    UiRenderer(const UiRenderer&) = delete;
    UiRenderer& operator=(const UiRenderer&) = delete;

    void beginFrame(int framebufferWidth, int framebufferHeight, Viewport contentViewport);
    void endFrame();

    // Restores the state expected by the solid primitives after a textured draw.
    void useSolidPrimitives();

    void fillRectangle(const Rect& rectangle, Color color);
    void fillVerticalGradient(const Rect& rectangle, Color top, Color bottom);
    void fillRoundedRectangle(const Rect& rectangle, float radius, Color color);
    void fillRoundedVerticalGradient(const Rect& rectangle, float radius, Color top, Color bottom);
    void fillCircle(Point center, float radius, Color color);
    void strokeCircle(Point center, float innerRadius, float outerRadius, Color color);
    void fillTriangle(Point first, Point second, Point third, Color color);

    // Replaces the current clip with a logical-canvas rectangle until clearClip().
    void setClip(Rect rectangle);
    void clearClip();
    // Scoped clips intersect with the active clip and restore it on popClip().
    void pushClip(Rect rectangle);
    void popClip();

    // Text sizes are specified in logical screen pixels.
    void drawText(Point origin, std::string_view text, float pixelHeight, Color color);
    void drawTextCentered(const Rect& bounds, std::string_view text, float pixelHeight, Color color);
    [[nodiscard]] float textWidth(std::string_view text, float pixelHeight) const;
    [[nodiscard]] float textLineHeight(float pixelHeight) const;
    void drawMkwSymbol(const Rect& bounds, MkwSymbol symbol, Color color);
    void drawTexture(const GXTexObj& texture, Rect destination, Color tint = {255, 255, 255, 255},
                     Rect uv = {0.0F, 0.0F, 1.0F, 1.0F});

private:
    struct FontGlyph {
        float textureLeft = 0.0F;
        float textureTop = 0.0F;
        float textureRight = 0.0F;
        float textureBottom = 0.0F;
        float xOffset = 0.0F;
        float yOffset = 0.0F;
        float xAdvance = 0.0F;
    };

    struct SymbolGlyph {
        float left = 0.0F;
        float top = 0.0F;
        float width = 0.0F;
        float height = 0.0F;
    };

    void configureGraphicsState();
    void configureTextState();
    void configureTexturedState(GXTexObj& texture);
    void configureNormalizedTextureState(GXTexObj& texture);
    void drawQuad(const Rect& rectangle, Color color);
    void drawRounded(const Rect& rectangle, float radius, Color top, Color bottom, bool gradient);
    void applyClip(Rect rectangle);

    void* fifoAllocation_ = nullptr;
    void* fifo_ = nullptr;
    void* uiFontPixels_ = nullptr;
    GXTexObj uiFontTexture_{};
    std::array<FontGlyph, 95> uiFontGlyphs_{};
    float uiFontAscent_ = 0.0F;
    float uiFontLineHeight_ = 0.0F;
    static constexpr std::size_t kMkwSymbolCount = 8;
    void* mkwSymbolPixels_[kMkwSymbolCount]{};
    GXTexObj mkwSymbolTextures_[kMkwSymbolCount]{};
    std::array<SymbolGlyph, kMkwSymbolCount> mkwSymbolGlyphs_{};
    int framebufferWidth_ = 0;
    int framebufferHeight_ = 0;
    Viewport contentViewport_{};
    Rect activeClip_{0.0F, 0.0F, kDesignWidth, kDesignHeight};
    std::array<Rect, 8> clipStack_{};
    std::size_t clipDepth_ = 0;
};

}  // namespace mii::ui
