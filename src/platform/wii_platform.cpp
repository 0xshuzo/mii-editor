#include "platform/wii_platform.hpp"

#include <gccore.h>
#include <ogc/conf.h>
#include <ogc/system.h>
#include <wiiuse/wpad.h>

namespace mii::platform {
namespace {

constexpr int kPrimaryRemote = WPAD_CHAN_0;
constexpr int kFirstFramebuffer = 0;
constexpr int kSecondFramebuffer = 1;

}  // namespace

WiiPlatform::WiiPlatform() = default;

void WiiPlatform::initialize() {
    // Make boot diagnostics visible in Dolphin's OSReport log channel.
    SYS_STDIO_Report(false);
    SYS_Report("[mii-editor] Video initialization started.\n");
    VIDEO_Init();
    auto* preferredMode = VIDEO_GetPreferredMode(nullptr);
    videoMode_ = preferredMode;
    framebufferWidth_ = preferredMode->fbWidth;
    framebufferHeight_ = preferredMode->efbHeight;
    const bool isWidescreen = CONF_GetAspectRatio() == CONF_ASPECT_16_9;
    if (isWidescreen) {
        contentViewport_ = {0.0F, 0.0F, static_cast<float>(framebufferWidth_),
                            static_cast<float>(framebufferHeight_)};
    } else {
        const float letterboxHeight = static_cast<float>(framebufferWidth_) * 9.0F / 16.0F;
        contentViewport_ = {0.0F, (static_cast<float>(framebufferHeight_) - letterboxHeight) / 2.0F,
                            static_cast<float>(framebufferWidth_), letterboxHeight};
    }

    for (int index = 0; index < kFrameBufferCount; ++index) {
        framebuffers_[index] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(preferredMode));
    }

    VIDEO_Configure(preferredMode);
    VIDEO_SetNextFramebuffer(framebuffers_[kFirstFramebuffer]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if ((preferredMode->viTVMode & VI_NON_INTERLACE) != 0) {
        VIDEO_WaitVSync();
    }

    WPAD_Init();
    WPAD_SetDataFormat(kPrimaryRemote, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(kPrimaryRemote, framebufferWidth_, framebufferHeight_);
    SYS_Report("[mii-editor] Video and Wii Remote initialization completed.\n");
}

void WiiPlatform::configureGraphics() {
    SYS_Report("[mii-editor] Configuring GX display copy.\n");
    auto* videoMode = static_cast<GXRModeObj*>(videoMode_);
    const GXColor clearColor{0, 0, 0, 255};
    GX_SetCopyClear(clearColor, 0x00ffffff);
    GX_SetDispCopySrc(0, 0, videoMode->fbWidth, videoMode->efbHeight);
    GX_SetDispCopyYScale(static_cast<float>(videoMode->xfbHeight) /
                          static_cast<float>(videoMode->efbHeight));
    GX_SetDispCopyDst(videoMode->fbWidth, videoMode->xfbHeight);
    GX_SetCopyFilter(videoMode->aa, videoMode->sample_pattern, GX_TRUE, videoMode->vfilter);
    GX_SetFieldMode(videoMode->field_rendering,
                    videoMode->viHeight == videoMode->xfbHeight * 2 ? GX_ENABLE : GX_DISABLE);
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetDispCopyGamma(GX_GM_1_0);
    GX_CopyDisp(framebuffers_[kFirstFramebuffer], GX_TRUE);
    GX_Flush();
    SYS_Report("[mii-editor] GX display copy configured.\n");
}

InputState WiiPlatform::pollInput() const {
    WPAD_ScanPads();

    InputState input;
    const u32 pressedButtons = WPAD_ButtonsDown(kPrimaryRemote);
    input.confirmPressed = (pressedButtons & WPAD_BUTTON_A) != 0;
    input.backPressed = (pressedButtons & WPAD_BUTTON_B) != 0;
    input.homePressed = (pressedButtons & WPAD_BUTTON_HOME) != 0;

    ir_t infrared{};
    WPAD_IR(kPrimaryRemote, &infrared);
    const bool insideContent = infrared.x >= contentViewport_.x &&
                               infrared.x <= contentViewport_.x + contentViewport_.width &&
                               infrared.y >= contentViewport_.y &&
                               infrared.y <= contentViewport_.y + contentViewport_.height;
    input.hasPointer = infrared.valid != 0 && insideContent;
    if (input.hasPointer) {
        input.pointer = {(static_cast<float>(infrared.x) - contentViewport_.x) * ui::kDesignWidth /
                             contentViewport_.width,
                         (static_cast<float>(infrared.y) - contentViewport_.y) * ui::kDesignHeight /
                             contentViewport_.height};
    }
    return input;
}

void WiiPlatform::present() {
    const int nextFramebuffer = visibleFramebuffer_ == kFirstFramebuffer ? kSecondFramebuffer : kFirstFramebuffer;
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(framebuffers_[nextFramebuffer], GX_TRUE);
    GX_Flush();
    VIDEO_SetNextFramebuffer(framebuffers_[nextFramebuffer]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    visibleFramebuffer_ = nextFramebuffer;
}

int WiiPlatform::framebufferWidth() const {
    return framebufferWidth_;
}

int WiiPlatform::framebufferHeight() const {
    return framebufferHeight_;
}

ui::Viewport WiiPlatform::contentViewport() const {
    return contentViewport_;
}

}  // namespace mii::platform
