#pragma once

#include "ui/ui_types.hpp"

namespace mii::platform {

struct InputState {
    bool confirmPressed = false;
    bool backPressed = false;
    bool homePressed = false;
    bool hasPointer = false;
    ui::Point pointer{};
};

// Owns the Wii video and Wii Remote setup, leaving application decisions to the UI layer.
class WiiPlatform {
public:
    WiiPlatform();

    void initialize();
    // Must be called after UiRenderer has initialized GX.
    void configureGraphics();
    [[nodiscard]] InputState pollInput() const;
    void present();

    [[nodiscard]] int framebufferWidth() const;
    [[nodiscard]] int framebufferHeight() const;
    [[nodiscard]] ui::Viewport contentViewport() const;

private:
    static constexpr int kFrameBufferCount = 2;

    void* framebuffers_[kFrameBufferCount]{};
    int visibleFramebuffer_ = 0;
    int framebufferWidth_ = 0;
    int framebufferHeight_ = 0;
    ui::Viewport contentViewport_{};
    void* videoMode_ = nullptr;
};

}  // namespace mii::platform
