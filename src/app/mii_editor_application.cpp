#include "app/mii_editor_application.hpp"

#include <ogc/system.h>

namespace mii::app {
namespace {

// Change only this development setting when a deterministic keyboard screenshot is needed.
#if defined(MII_EDITOR_RENDERER_DIAGNOSTIC) && MII_EDITOR_RENDERER_DIAGNOSTIC
constexpr ui::Screen kDevelopmentInitialScreen = ui::Screen::RendererDiagnostics;
#else
constexpr ui::Screen kDevelopmentInitialScreen = ui::Screen::MiiList;
#endif

}  // namespace

MiiEditorApplication::MiiEditorApplication(platform::WiiPlatform& platform, ui::UiRenderer& renderer)
    : platform_(platform), renderer_(renderer), screen_(kDevelopmentInitialScreen) {}

void MiiEditorApplication::run() {
    SYS_Report("[mii-editor] Entering application loop.\n");
    while (running_) {
        const platform::InputState input = platform_.pollInput();
        handleInput(input);
        render(input);
    }
}

void MiiEditorApplication::handleInput(const platform::InputState& input) {
    if (input.homePressed) {
        running_ = false;
        return;
    }

    if (screen_ == ui::Screen::MiiList && input.confirmPressed && input.hasPointer &&
        ui_.miiNameField().contains(input.pointer)) {
        openNameEditor();
        return;
    }

    (void)input.backPressed;
    // TODO: Route Mii selection, keyboard presses, Back, Save, Cancel, and Settings here.
}

void MiiEditorApplication::openNameEditor() {
    screen_ = ui::Screen::NameEditor;
    // TODO: Initialize a temporary editable name from the selected Mii.
}

void MiiEditorApplication::closeNameEditor() {
    screen_ = ui::Screen::MiiList;
    // TODO: Ask whether unsaved name edits should be discarded.
}

void MiiEditorApplication::saveMiiName() {
    // TODO: Commit the pending name to the active Mii data source.
}

void MiiEditorApplication::discardPendingChanges() {
    // TODO: Restore the editable fields from the selected Mii.
}

void MiiEditorApplication::render(const platform::InputState& input) {
    renderer_.beginFrame(platform_.framebufferWidth(), platform_.framebufferHeight(),
                         platform_.contentViewport());
    ui_.render(renderer_, miis_, screen_, input.hasPointer, input.pointer);
    renderer_.endFrame();
    platform_.present();
}

}  // namespace mii::app
