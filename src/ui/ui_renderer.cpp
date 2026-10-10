#include "ui/ui_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <malloc.h>

#include <gccore.h>
#include <ogc/gu.h>
#include <ogc/system.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace mii::ui {
namespace {

constexpr std::size_t kFifoSize = 256 * 1024;
constexpr int kMkwSymbolTextureSize = 64;
constexpr int kMkwSymbolPixelHeight = 42;
constexpr int kUiFontAtlasSize = 512;
constexpr float kUiFontBakeHeight = 32.0F;
constexpr int kFirstUiCodepoint = 32;
constexpr int kUiGlyphCount = 95;
constexpr int kCircleSegments = 24;
constexpr int kRoundedCornerSegments = 6;
constexpr float kFullCircleRadians = 6.28318530718F;
constexpr float kQuarterCircleRadians = 1.57079632679F;

extern "C" {
extern const unsigned char _binary_ctmkf_ttf_start[];
extern const unsigned char _binary_ctmkf_ttf_end[];
extern const unsigned char _binary_nunito_semibold_ttf_start[];
extern const unsigned char _binary_nunito_semibold_ttf_end[];
}

constexpr std::size_t symbolIndex(const MkwSymbol symbol) {
    return static_cast<std::size_t>(symbol);
}

Rect intersect(const Rect first, const Rect second) {
    const float left = std::max(first.x, second.x);
    const float top = std::max(first.y, second.y);
    const float right = std::min(first.right(), second.right());
    const float bottom = std::min(first.bottom(), second.bottom());
    return {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)};
}

constexpr int codepointFor(const MkwSymbol symbol) {
    switch (symbol) {
        case MkwSymbol::Star: return 0xF061;
        case MkwSymbol::WiimoteTwo: return 0xF030;
        case MkwSymbol::WiimoteA: return 0xF034;
        case MkwSymbol::WiimoteB: return 0xF041;
        case MkwSymbol::WiimotePlus: return 0xF044;
        case MkwSymbol::Coin: return 0xF068;
        case MkwSymbol::Balloon: return 0xF069;
        case MkwSymbol::Trophy: return 0xF06A;
    }
    return 0;
}

void storeIa8Pixel(u8* destination, const int textureWidth, const int x, const int y, const u8 alpha) {
    const int tilesPerRow = textureWidth / 4;
    const int tileIndex = (y / 4) * tilesPerRow + x / 4;
    const int pixelInTile = (y % 4) * 4 + x % 4;
    const int offset = tileIndex * 32 + pixelInTile * 2;
    destination[offset] = alpha;
    destination[offset + 1] = 0xFF;
}

void setColor(const Color color) {
    GX_Color4u8(color.red, color.green, color.blue, color.alpha);
}

Color interpolate(const Color top, const Color bottom, const float amount) {
    const float t = std::clamp(amount, 0.0F, 1.0F);
    const float inverse = 1.0F - t;
    return {static_cast<u8>(top.red * inverse + bottom.red * t),
            static_cast<u8>(top.green * inverse + bottom.green * t),
            static_cast<u8>(top.blue * inverse + bottom.blue * t),
            static_cast<u8>(top.alpha * inverse + bottom.alpha * t)};
}

}  // namespace

UiRenderer::UiRenderer() {
    SYS_Report("[mii-editor] Initializing GX renderer.\n");
    fifoAllocation_ = memalign(32, kFifoSize);
    fifo_ = MEM_K0_TO_K1(fifoAllocation_);
    std::memset(fifo_, 0, kFifoSize);
    GX_Init(fifo_, kFifoSize);

    stbtt_fontinfo uiFont{};
    const int uiFontOffset = stbtt_GetFontOffsetForIndex(_binary_nunito_semibold_ttf_start, 0);
    if (!stbtt_InitFont(&uiFont, _binary_nunito_semibold_ttf_start, uiFontOffset)) {
        SYS_Report("[mii-editor] Could not initialize the UI font.\n");
    }
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&uiFont, &ascent, &descent, &lineGap);
    const float fontScale = stbtt_ScaleForPixelHeight(&uiFont, kUiFontBakeHeight);
    uiFontAscent_ = static_cast<float>(ascent) * fontScale;
    uiFontLineHeight_ = static_cast<float>(ascent - descent + lineGap) * fontScale;

    auto* fontPixels = static_cast<u8*>(memalign(32, kUiFontAtlasSize * kUiFontAtlasSize));
    std::memset(fontPixels, 0, kUiFontAtlasSize * kUiFontAtlasSize);
    stbtt_bakedchar baked[kUiGlyphCount]{};
    const int bakeResult = stbtt_BakeFontBitmap(
        _binary_nunito_semibold_ttf_start,
        stbtt_GetFontOffsetForIndex(_binary_nunito_semibold_ttf_start, 0), kUiFontBakeHeight,
        fontPixels, kUiFontAtlasSize, kUiFontAtlasSize, kFirstUiCodepoint, kUiGlyphCount, baked);
    SYS_Report("[mii-editor] UI font atlas result: %d.\n", bakeResult);
    if (bakeResult <= 0) SYS_Report("[mii-editor] UI font atlas is incomplete.\n");
    uiFontPixels_ = memalign(32, kUiFontAtlasSize * kUiFontAtlasSize * 2);
    std::memset(uiFontPixels_, 0, kUiFontAtlasSize * kUiFontAtlasSize * 2);
    for (int glyph = 0; glyph < kUiGlyphCount; ++glyph) {
        const stbtt_bakedchar& source = baked[glyph];
        FontGlyph& destination = uiFontGlyphs_[static_cast<std::size_t>(glyph)];
        destination = {static_cast<float>(source.x0), static_cast<float>(source.y0),
                       static_cast<float>(source.x1), static_cast<float>(source.y1),
                       source.xoff, source.yoff, source.xadvance};
    }
    for (int y = 0; y < kUiFontAtlasSize; ++y)
        for (int x = 0; x < kUiFontAtlasSize; ++x)
            storeIa8Pixel(static_cast<u8*>(uiFontPixels_), kUiFontAtlasSize, x, y,
                          fontPixels[y * kUiFontAtlasSize + x]);
    std::free(fontPixels);
    DCFlushRange(uiFontPixels_, kUiFontAtlasSize * kUiFontAtlasSize * 2);
    GX_InitTexObj(&uiFontTexture_, uiFontPixels_, kUiFontAtlasSize, kUiFontAtlasSize,
                  GX_TF_IA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&uiFontTexture_, GX_LINEAR, GX_LINEAR, 0.0F, 0.0F, 0.0F,
                     GX_FALSE, GX_FALSE, GX_ANISO_1);

    stbtt_fontinfo ctmkf{};
    const int ctmkfOffset = stbtt_GetFontOffsetForIndex(_binary_ctmkf_ttf_start, 0);
    stbtt_InitFont(&ctmkf, _binary_ctmkf_ttf_start, ctmkfOffset);
    const float symbolScale = stbtt_ScaleForPixelHeight(&ctmkf, kMkwSymbolPixelHeight);
    for (const MkwSymbol symbol : {MkwSymbol::Star, MkwSymbol::WiimoteTwo, MkwSymbol::WiimoteA,
                                   MkwSymbol::WiimoteB, MkwSymbol::WiimotePlus, MkwSymbol::Coin,
                                   MkwSymbol::Balloon, MkwSymbol::Trophy}) {
        const std::size_t index = symbolIndex(symbol);
        int glyphWidth = 0;
        int glyphHeight = 0;
        int xOffset = 0;
        int yOffset = 0;
        unsigned char* glyph = stbtt_GetCodepointBitmap(&ctmkf, symbolScale, symbolScale,
                                                         codepointFor(symbol), &glyphWidth,
                                                         &glyphHeight, &xOffset, &yOffset);
        mkwSymbolPixels_[index] = memalign(32, kMkwSymbolTextureSize * kMkwSymbolTextureSize * 2);
        std::memset(mkwSymbolPixels_[index], 0, kMkwSymbolTextureSize * kMkwSymbolTextureSize * 2);
        if (glyph != nullptr && glyphWidth > 0 && glyphHeight > 0 &&
            glyphWidth <= kMkwSymbolTextureSize - 2 && glyphHeight <= kMkwSymbolTextureSize - 2) {
            const int glyphX = (kMkwSymbolTextureSize - glyphWidth) / 2;
            const int glyphY = (kMkwSymbolTextureSize - glyphHeight) / 2;
            for (int y = 0; y < glyphHeight; ++y)
                for (int x = 0; x < glyphWidth; ++x)
                    storeIa8Pixel(static_cast<u8*>(mkwSymbolPixels_[index]), kMkwSymbolTextureSize,
                                  glyphX + x, glyphY + y, glyph[y * glyphWidth + x]);
            mkwSymbolGlyphs_[index] = {static_cast<float>(glyphX), static_cast<float>(glyphY),
                                       static_cast<float>(glyphWidth), static_cast<float>(glyphHeight)};
        } else {
            SYS_Report("[mii-editor] CTMKF glyph %x is unavailable.\n", codepointFor(symbol));
        }
        if (glyph != nullptr) stbtt_FreeBitmap(glyph, nullptr);
        DCFlushRange(mkwSymbolPixels_[index], kMkwSymbolTextureSize * kMkwSymbolTextureSize * 2);
        GX_InitTexObj(&mkwSymbolTextures_[index], mkwSymbolPixels_[index], kMkwSymbolTextureSize,
                      kMkwSymbolTextureSize, GX_TF_IA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjLOD(&mkwSymbolTextures_[index], GX_LINEAR, GX_LINEAR, 0.0F, 0.0F, 0.0F,
                         GX_FALSE, GX_FALSE, GX_ANISO_1);
    }
    GX_InvalidateTexAll();
    configureGraphicsState();
    SYS_Report("[mii-editor] GX renderer initialized.\n");
}

UiRenderer::~UiRenderer() {
    for (void* pixels : mkwSymbolPixels_) std::free(pixels);
    std::free(uiFontPixels_);
    std::free(fifoAllocation_);
}

void UiRenderer::configureGraphicsState() {
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE,
                   GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
}

void UiRenderer::configureTextState() {
    configureTexturedState(uiFontTexture_);
}

void UiRenderer::configureTexturedState(GXTexObj& texture) {
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetNumChans(1);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE,
                   GX_AF_NONE);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_LoadTexObj(&texture, GX_TEXMAP0);
}

void UiRenderer::configureNormalizedTextureState(GXTexObj& texture) {
    configureTexturedState(texture);
    GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_FALSE, 0, 0);
}

void UiRenderer::beginFrame(const int framebufferWidth, const int framebufferHeight,
                            const Viewport contentViewport) {
    // The Wii Remote cursor uses a textured vertex format. Restore the UI's
    // untextured format before emitting this frame's solid primitives.
    configureGraphicsState();

    framebufferWidth_ = framebufferWidth;
    framebufferHeight_ = framebufferHeight;
    contentViewport_ = contentViewport;

    Mtx44 projection;
    Mtx modelView;
    guOrtho(projection, 0.0F, kDesignHeight, 0.0F, kDesignWidth, 0.0F, 1.0F);
    guMtxIdentity(modelView);
    GX_SetViewport(contentViewport.x, contentViewport.y, contentViewport.width,
                   contentViewport.height, 0.0F, 1.0F);
    clipDepth_ = 0;
    clearClip();
    GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
    GX_LoadPosMtxImm(modelView, GX_PNMTX0);
    GX_SetCurrentMtx(GX_PNMTX0);
    GX_InvVtxCache();
}

void UiRenderer::endFrame() {
    GX_DrawDone();
}

void UiRenderer::useSolidPrimitives() {
    configureGraphicsState();
}

void UiRenderer::drawQuad(const Rect& rectangle, const Color color) {
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(rectangle.x, rectangle.y, 0.0F);
    setColor(color);
    GX_Position3f32(rectangle.x + rectangle.width, rectangle.y, 0.0F);
    setColor(color);
    GX_Position3f32(rectangle.x + rectangle.width, rectangle.y + rectangle.height, 0.0F);
    setColor(color);
    GX_Position3f32(rectangle.x, rectangle.y + rectangle.height, 0.0F);
    setColor(color);
}

void UiRenderer::fillRectangle(const Rect& rectangle, const Color color) {
    if (rectangle.width <= 0.0F || rectangle.height <= 0.0F) return;
    configureGraphicsState();
    drawQuad(rectangle, color);
}

void UiRenderer::fillVerticalGradient(const Rect& rectangle, const Color top, const Color bottom) {
    if (rectangle.width <= 0.0F || rectangle.height <= 0.0F) return;
    configureGraphicsState();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(rectangle.x, rectangle.y, 0.0F);
    setColor(top);
    GX_Position3f32(rectangle.x + rectangle.width, rectangle.y, 0.0F);
    setColor(top);
    GX_Position3f32(rectangle.x + rectangle.width, rectangle.y + rectangle.height, 0.0F);
    setColor(bottom);
    GX_Position3f32(rectangle.x, rectangle.y + rectangle.height, 0.0F);
    setColor(bottom);
}

void UiRenderer::fillCircle(const Point center, const float radius, const Color color) {
    if (radius <= 0.0F) return;
    configureGraphicsState();
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, kCircleSegments + 2);
    GX_Position3f32(center.x, center.y, 0.0F);
    setColor(color);
    for (int segment = 0; segment <= kCircleSegments; ++segment) {
        const float angle = static_cast<float>(segment) / static_cast<float>(kCircleSegments) * kFullCircleRadians;
        GX_Position3f32(center.x + radius * cosf(angle), center.y + radius * sinf(angle), 0.0F);
        setColor(color);
    }
}

void UiRenderer::strokeCircle(const Point center, const float innerRadius, const float outerRadius,
                              const Color color) {
    if (outerRadius <= 0.0F || outerRadius <= innerRadius) return;
    configureGraphicsState();
    const float inner = std::max(0.0F, innerRadius);
    GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, (kCircleSegments + 1) * 2);
    for (int segment = 0; segment <= kCircleSegments; ++segment) {
        const float angle = static_cast<float>(segment) / static_cast<float>(kCircleSegments) * kFullCircleRadians;
        const float cosine = cosf(angle);
        const float sine = sinf(angle);
        GX_Position3f32(center.x + cosine * inner, center.y + sine * inner, 0.0F);
        setColor(color);
        GX_Position3f32(center.x + cosine * outerRadius, center.y + sine * outerRadius, 0.0F);
        setColor(color);
    }
}

void UiRenderer::fillRoundedRectangle(const Rect& rectangle, const float radius, const Color color) {
    drawRounded(rectangle, radius, color, color, false);
}

void UiRenderer::fillRoundedVerticalGradient(const Rect& rectangle, const float radius,
                                             const Color top, const Color bottom) {
    drawRounded(rectangle, radius, top, bottom, true);
}

void UiRenderer::drawRounded(const Rect& rectangle, const float radius, const Color top,
                             const Color bottom, const bool gradient) {
    if (rectangle.width <= 0.0F || rectangle.height <= 0.0F) return;

    const float finalRadius = std::clamp(radius, 0.0F,
        std::min(rectangle.width, rectangle.height) / 2.0F);
    if (finalRadius <= 0.0F) {
        if (gradient) fillVerticalGradient(rectangle, top, bottom);
        else fillRectangle(rectangle, top);
        return;
    }
    configureGraphicsState();

    // One clockwise boundary is shared by the flat and gradient variants.  Each
    // join point is emitted only once; the last submitted point explicitly
    // repeats the first point so GX closes the final fan sector.
    constexpr std::size_t kMaximumPerimeterPoints = 4 * (kRoundedCornerSegments + 1);
    std::array<Point, kMaximumPerimeterPoints> perimeter{};
    std::size_t pointCount = 0;
    const std::array<Point, 4> centers{{
        {rectangle.right() - finalRadius, rectangle.y + finalRadius},
        {rectangle.right() - finalRadius, rectangle.bottom() - finalRadius},
        {rectangle.x + finalRadius, rectangle.bottom() - finalRadius},
        {rectangle.x + finalRadius, rectangle.y + finalRadius},
    }};

    for (int corner = 0; corner < 4; ++corner) {
        const float start = -kQuarterCircleRadians + static_cast<float>(corner) * kQuarterCircleRadians;
        for (int segment = 0; segment <= kRoundedCornerSegments; ++segment) {
            // The first point of every following arc is the preceding arc's
            // endpoint, so skip it rather than generating degenerate fan wedges.
            if (corner > 0 && segment == 0) continue;
            const float angle = start + static_cast<float>(segment) /
                static_cast<float>(kRoundedCornerSegments) * kQuarterCircleRadians;
            perimeter[pointCount++] = {centers[static_cast<std::size_t>(corner)].x + cosf(angle) * finalRadius,
                                       centers[static_cast<std::size_t>(corner)].y + sinf(angle) * finalRadius};
        }
    }

    const auto colorAt = [&](const float y) {
        return gradient ? interpolate(top, bottom, (y - rectangle.y) / rectangle.height) : top;
    };
    static bool loggedMesh = false;
    if (!loggedMesh) {
        SYS_Report("[mii-editor] Rounded shapes use a closed %u-point perimeter (%u fan vertices).\n",
                   static_cast<unsigned>(pointCount), static_cast<unsigned>(pointCount + 2));
        loggedMesh = true;
    }
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, static_cast<u16>(pointCount + 2));
    const Point center{rectangle.x + rectangle.width / 2.0F, rectangle.y + rectangle.height / 2.0F};
    GX_Position3f32(center.x, center.y, 0.0F);
    setColor(colorAt(center.y));
    for (std::size_t index = 0; index < pointCount; ++index) {
        GX_Position3f32(perimeter[index].x, perimeter[index].y, 0.0F);
        setColor(colorAt(perimeter[index].y));
    }
    GX_Position3f32(perimeter[0].x, perimeter[0].y, 0.0F);
    setColor(colorAt(perimeter[0].y));
}

void UiRenderer::fillTriangle(const Point first, const Point second, const Point third, const Color color) {
    configureGraphicsState();
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GX_Position3f32(first.x, first.y, 0.0F);
    setColor(color);
    GX_Position3f32(second.x, second.y, 0.0F);
    setColor(color);
    GX_Position3f32(third.x, third.y, 0.0F);
    setColor(color);
}

void UiRenderer::setClip(const Rect rectangle) {
    activeClip_ = intersect({0.0F, 0.0F, kDesignWidth, kDesignHeight}, rectangle);
    applyClip(activeClip_);
}

void UiRenderer::clearClip() {
    activeClip_ = {0.0F, 0.0F, kDesignWidth, kDesignHeight};
    clipDepth_ = 0;
    applyClip(activeClip_);
}

void UiRenderer::pushClip(const Rect rectangle) {
    if (clipDepth_ < clipStack_.size()) clipStack_[clipDepth_++] = activeClip_;
    activeClip_ = intersect(activeClip_, rectangle);
    applyClip(activeClip_);
}

void UiRenderer::popClip() {
    if (clipDepth_ == 0) {
        clearClip();
        return;
    }
    activeClip_ = clipStack_[--clipDepth_];
    applyClip(activeClip_);
}

void UiRenderer::applyClip(const Rect rectangle) {
    const float scaleX = contentViewport_.width / kDesignWidth;
    const float scaleY = contentViewport_.height / kDesignHeight;
    const int viewportLeft = std::max(0, static_cast<int>(std::floor(contentViewport_.x)));
    const int viewportTop = std::max(0, static_cast<int>(std::floor(contentViewport_.y)));
    const int viewportRight = std::min(framebufferWidth_, static_cast<int>(std::ceil(contentViewport_.x + contentViewport_.width)));
    const int viewportBottom = std::min(framebufferHeight_, static_cast<int>(std::ceil(contentViewport_.y + contentViewport_.height)));
    const int left = std::clamp(static_cast<int>(std::floor(contentViewport_.x + rectangle.x * scaleX)),
                                viewportLeft, viewportRight);
    const int top = std::clamp(static_cast<int>(std::floor(contentViewport_.y + rectangle.y * scaleY)),
                               viewportTop, viewportBottom);
    const int right = std::clamp(static_cast<int>(std::ceil(contentViewport_.x + rectangle.right() * scaleX)),
                                 viewportLeft, viewportRight);
    const int bottom = std::clamp(static_cast<int>(std::ceil(contentViewport_.y + rectangle.bottom() * scaleY)),
                                  viewportTop, viewportBottom);
    GX_SetScissor(static_cast<u32>(left), static_cast<u32>(top), static_cast<u32>(std::max(0, right - left)),
                  static_cast<u32>(std::max(0, bottom - top)));
}

float UiRenderer::textWidth(const std::string_view text, const float pixelHeight) const {
    const float scale = pixelHeight / kUiFontBakeHeight;
    float width = 0.0F;
    for (const char character : text) {
        const int index = static_cast<unsigned char>(character) - kFirstUiCodepoint;
        if (index >= 0 && index < kUiGlyphCount)
            width += uiFontGlyphs_[static_cast<std::size_t>(index)].xAdvance * scale;
    }
    return width;
}

float UiRenderer::textLineHeight(const float pixelHeight) const {
    return uiFontLineHeight_ * (pixelHeight / kUiFontBakeHeight);
}

void UiRenderer::drawText(const Point origin, const std::string_view text, const float pixelHeight,
                          const Color color) {
    configureTextState();
    const float scale = pixelHeight / kUiFontBakeHeight;
    float penX = origin.x;
    const float baseline = origin.y + uiFontAscent_ * scale;
    for (const char character : text) {
        const int index = static_cast<unsigned char>(character) - kFirstUiCodepoint;
        if (index < 0 || index >= kUiGlyphCount) continue;
        const FontGlyph& glyph = uiFontGlyphs_[static_cast<std::size_t>(index)];
        const float left = penX + glyph.xOffset * scale;
        const float top = baseline + glyph.yOffset * scale;
        const float right = left + (glyph.textureRight - glyph.textureLeft) * scale;
        const float bottom = top + (glyph.textureBottom - glyph.textureTop) * scale;

        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position2s16(static_cast<s16>(std::lround(left)), static_cast<s16>(std::lround(top)));
        setColor(color); GX_TexCoord2f32(glyph.textureLeft, glyph.textureTop);
        GX_Position2s16(static_cast<s16>(std::lround(right)), static_cast<s16>(std::lround(top)));
        setColor(color); GX_TexCoord2f32(glyph.textureRight, glyph.textureTop);
        GX_Position2s16(static_cast<s16>(std::lround(right)), static_cast<s16>(std::lround(bottom)));
        setColor(color); GX_TexCoord2f32(glyph.textureRight, glyph.textureBottom);
        GX_Position2s16(static_cast<s16>(std::lround(left)), static_cast<s16>(std::lround(bottom)));
        setColor(color); GX_TexCoord2f32(glyph.textureLeft, glyph.textureBottom);
        penX += glyph.xAdvance * scale;
    }
}

void UiRenderer::drawTextCentered(const Rect& bounds, const std::string_view text, const float pixelHeight,
                                  const Color color) {
    drawText({bounds.x + (bounds.width - textWidth(text, pixelHeight)) / 2.0F,
              bounds.y + (bounds.height - textLineHeight(pixelHeight)) / 2.0F}, text, pixelHeight, color);
}

void UiRenderer::drawMkwSymbol(const Rect& bounds, const MkwSymbol symbol, const Color color) {
    const SymbolGlyph glyph = mkwSymbolGlyphs_[symbolIndex(symbol)];
    if (glyph.width <= 0.0F || glyph.height <= 0.0F) return;

    const float gutter = 1.0F;
    const float sourceLeft = std::max(0.0F, glyph.left - gutter);
    const float sourceTop = std::max(0.0F, glyph.top - gutter);
    const float sourceRight = std::min(static_cast<float>(kMkwSymbolTextureSize), glyph.left + glyph.width + gutter);
    const float sourceBottom = std::min(static_cast<float>(kMkwSymbolTextureSize), glyph.top + glyph.height + gutter);
    const float sourceWidth = sourceRight - sourceLeft;
    const float sourceHeight = sourceBottom - sourceTop;
    const float fit = std::min(bounds.width / sourceWidth, bounds.height / sourceHeight);
    const Rect fitted{bounds.x + (bounds.width - sourceWidth * fit) / 2.0F,
                      bounds.y + (bounds.height - sourceHeight * fit) / 2.0F,
                      sourceWidth * fit, sourceHeight * fit};

    configureTexturedState(mkwSymbolTextures_[symbolIndex(symbol)]);
    const float left = fitted.x;
    const float top = fitted.y;
    const float right = fitted.right();
    const float bottom = fitted.bottom();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(static_cast<s16>(std::lround(left)), static_cast<s16>(std::lround(top)));
    setColor(color); GX_TexCoord2f32(sourceLeft, sourceTop);
    GX_Position2s16(static_cast<s16>(std::lround(right)), static_cast<s16>(std::lround(top)));
    setColor(color); GX_TexCoord2f32(sourceRight, sourceTop);
    GX_Position2s16(static_cast<s16>(std::lround(right)), static_cast<s16>(std::lround(bottom)));
    setColor(color); GX_TexCoord2f32(sourceRight, sourceBottom);
    GX_Position2s16(static_cast<s16>(std::lround(left)), static_cast<s16>(std::lround(bottom)));
    setColor(color); GX_TexCoord2f32(sourceLeft, sourceBottom);
}

void UiRenderer::drawTexture(const GXTexObj& texture, const Rect destination, const Color tint,
                             const Rect uv) {
    configureNormalizedTextureState(const_cast<GXTexObj&>(texture));
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(static_cast<s16>(std::lround(destination.x)), static_cast<s16>(std::lround(destination.y)));
    setColor(tint); GX_TexCoord2f32(uv.x, uv.y);
    GX_Position2s16(static_cast<s16>(std::lround(destination.right())), static_cast<s16>(std::lround(destination.y)));
    setColor(tint); GX_TexCoord2f32(uv.right(), uv.y);
    GX_Position2s16(static_cast<s16>(std::lround(destination.right())), static_cast<s16>(std::lround(destination.bottom())));
    setColor(tint); GX_TexCoord2f32(uv.right(), uv.bottom());
    GX_Position2s16(static_cast<s16>(std::lround(destination.x)), static_cast<s16>(std::lround(destination.bottom())));
    setColor(tint); GX_TexCoord2f32(uv.x, uv.bottom());
}

}  // namespace mii::ui
