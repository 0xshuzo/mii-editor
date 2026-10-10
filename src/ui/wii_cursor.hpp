#pragma once

#include <gccore.h>

#include "ui/ui_renderer.hpp"

namespace mii::ui {

class WiiCursor {
public:
    WiiCursor();
    ~WiiCursor();

    // The presentation build deliberately uses one static cursor appearance.
    void update(bool pointerValid, Point pointer);
    void render(UiRenderer& renderer) const;

private:
    struct Texture { GXTexObj object{}; void* pixels = nullptr; };

    void initializeTexture(Texture& texture, const unsigned char* source);

    Texture hand_{};
    Texture mask_{};
    Point pointer_{};
    bool visible_ = false;
};

}  // namespace mii::ui
