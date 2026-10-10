#include "ui/ui_assets.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include <gccore.h>
#include <ogc/cache.h>
#include <ogc/system.h>

#include "ui/ui_renderer.hpp"
#include "ui_artwork/ui_artwork_metadata.hpp"

namespace mii::ui {
namespace {

extern "C" {
extern const unsigned char _binary_hero_rgba_start[];
extern const unsigned char _binary_luca_rgba_start[];
extern const unsigned char _binary_sora_rgba_start[];
extern const unsigned char _binary_mika_rgba_start[];
extern const unsigned char _binary_rex_rgba_start[];
extern const unsigned char _binary_nina_rgba_start[];
extern const unsigned char _binary_tobi_rgba_start[];
}

struct SourceImage {
    const unsigned char* pixels;
    generated::ArtworkMetadata metadata;
};

const std::array<SourceImage, 7> kSources{{
    {_binary_hero_rgba_start, generated::kArtworkMetadata[0]},
    {_binary_luca_rgba_start, generated::kArtworkMetadata[1]},
    {_binary_sora_rgba_start, generated::kArtworkMetadata[2]},
    {_binary_mika_rgba_start, generated::kArtworkMetadata[3]},
    {_binary_rex_rgba_start, generated::kArtworkMetadata[4]},
    {_binary_nina_rgba_start, generated::kArtworkMetadata[5]},
    {_binary_tobi_rgba_start, generated::kArtworkMetadata[6]},
}};

bool storeRgba8Pixel(unsigned char* destination, const std::size_t byteCount,
                     const int textureWidth, const int textureHeight, const int x, const int y,
                     const unsigned char* source) {
    if (x < 0 || x >= textureWidth || y < 0 || y >= textureHeight || textureWidth % 4 != 0 ||
        textureHeight % 4 != 0) return false;
    const int blockIndex = (y / 4) * (textureWidth / 4) + x / 4;
    const int pixelIndex = (y % 4) * 4 + x % 4;
    const std::size_t offset = static_cast<std::size_t>(blockIndex) * 64U;
    if (offset + 64U > byteCount) return false;
    destination[offset + pixelIndex * 2] = source[3];
    destination[offset + pixelIndex * 2 + 1] = source[0];
    destination[offset + 32 + pixelIndex * 2] = source[1];
    destination[offset + 32 + pixelIndex * 2 + 1] = source[2];
    return true;
}

constexpr int alignToRgba8Tile(const int value) {
    return (value + 3) & ~3;
}

}  // namespace

UiAssets::~UiAssets() {
    for (Texture& texture : textures_) std::free(texture.pixels);
}

void UiAssets::initialize() {
    for (std::size_t index = 0; index < textures_.size(); ++index) {
        const SourceImage& source = kSources[index];
        Texture& texture = textures_[index];
        texture.contentWidth = source.metadata.width;
        texture.contentHeight = source.metadata.height;
        if (source.metadata.byteCount != static_cast<std::size_t>(texture.contentWidth) *
                                         static_cast<std::size_t>(texture.contentHeight) * 4U) {
            SYS_Report("[mii-editor] Artwork metadata byte count is invalid.\n");
            continue;
        }
        // A one-pixel edge gutter prevents linear sampling from leaking the
        // transparent padding into the artwork.  The resulting storage remains
        // aligned to GX RGBA8's 4x4 tile layout.
        texture.contentOffsetX = 1;
        texture.contentOffsetY = 1;
        texture.storageWidth = alignToRgba8Tile(texture.contentWidth + texture.contentOffsetX * 2);
        texture.storageHeight = alignToRgba8Tile(texture.contentHeight + texture.contentOffsetY * 2);
        texture.byteCount = static_cast<std::size_t>(texture.storageWidth) *
                            static_cast<std::size_t>(texture.storageHeight) * 4U;
        texture.pixels = memalign(32, texture.byteCount);
        if (texture.pixels == nullptr) {
            SYS_Report("[mii-editor] Artwork texture allocation failed (%dx%d storage).\n",
                       texture.storageWidth, texture.storageHeight);
            continue;
        }
        std::memset(texture.pixels, 0, texture.byteCount);

        auto* tiledPixels = static_cast<unsigned char*>(texture.pixels);
        bool wroteAllPixels = true;
        for (int y = 0; y < texture.storageHeight; ++y) {
            const int sourceY = std::clamp(y - texture.contentOffsetY, 0, texture.contentHeight - 1);
            for (int x = 0; x < texture.storageWidth; ++x) {
                const int sourceX = std::clamp(x - texture.contentOffsetX, 0, texture.contentWidth - 1);
                if (!storeRgba8Pixel(tiledPixels, texture.byteCount, texture.storageWidth,
                                     texture.storageHeight, x, y,
                                     source.pixels + (sourceY * texture.contentWidth + sourceX) * 4)) {
                    wroteAllPixels = false;
                    break;
                }
            }
            if (!wroteAllPixels) break;
        }
        if (!wroteAllPixels) {
            SYS_Report("[mii-editor] Artwork texture tile write exceeded its allocation.\n");
            std::free(texture.pixels);
            texture.pixels = nullptr;
            continue;
        }

        DCFlushRange(texture.pixels, texture.byteCount);
        GX_InitTexObj(&texture.object, texture.pixels, static_cast<u16>(texture.storageWidth),
                      static_cast<u16>(texture.storageHeight), GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjLOD(&texture.object, GX_LINEAR, GX_LINEAR, 0.0F, 0.0F, 0.0F,
                         GX_FALSE, GX_FALSE, GX_ANISO_1);
        texture.valid = true;
    }
    GX_InvalidateTexAll();
}

void UiAssets::draw(UiRenderer& renderer, const ArtworkId artwork, const Rect destination) const {
    const Texture& texture = textures_[static_cast<std::size_t>(artwork)];
    if (!texture.valid) return;
    const Rect uv{static_cast<float>(texture.contentOffsetX) / texture.storageWidth,
                  static_cast<float>(texture.contentOffsetY) / texture.storageHeight,
                  static_cast<float>(texture.contentWidth) / texture.storageWidth,
                  static_cast<float>(texture.contentHeight) / texture.storageHeight};
    renderer.drawTexture(texture.object, destination, {255, 255, 255, 255}, uv);
}

}  // namespace mii::ui
