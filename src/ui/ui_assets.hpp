#pragma once

#include <array>
#include <cstddef>

#include <gccore.h>

#include "ui/ui_types.hpp"

namespace mii::ui {

class UiRenderer;

enum class ArtworkId : std::size_t {
    HeroSora,
    Luca,
    Sora,
    Mika,
    Rex,
    Nina,
    Tobi,
};

// Owns the pre-converted static artwork used by this presentation-only build.
class UiAssets {
public:
    UiAssets() = default;
    ~UiAssets();

    UiAssets(const UiAssets&) = delete;
    UiAssets& operator=(const UiAssets&) = delete;

    void initialize();
    void draw(UiRenderer& renderer, ArtworkId artwork, Rect destination) const;

private:
    struct Texture {
        GXTexObj object{};
        void* pixels = nullptr;
        int contentWidth = 0;
        int contentHeight = 0;
        int storageWidth = 0;
        int storageHeight = 0;
        int contentOffsetX = 0;
        int contentOffsetY = 0;
        std::size_t byteCount = 0;
        bool valid = false;
    };

    static constexpr std::size_t kTextureCount = 7;
    std::array<Texture, kTextureCount> textures_{};
};

}  // namespace mii::ui
