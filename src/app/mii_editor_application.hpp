#pragma once

#include "model/placeholder_mii.hpp"
#include "platform/wii_platform.hpp"
#include "ui/mii_editor_ui.hpp"
#include "ui/ui_renderer.hpp"

namespace mii::app {

// Coordinates UI state. Domain operations are deliberately declared now so
// input handling does not need to be restructured when Mii storage arrives.
class MiiEditorApplication {
public:
    MiiEditorApplication(platform::WiiPlatform& platform, ui::UiRenderer& renderer);

    void run();

private:
    void handleInput(const platform::InputState& input);
    void openNameEditor();
    void closeNameEditor();
    void saveMiiName();
    void discardPendingChanges();
    void render(const platform::InputState& input);

    platform::WiiPlatform& platform_;
    ui::UiRenderer& renderer_;
    model::PlaceholderMiiRepository miis_;
    ui::MiiEditorUi ui_;
    ui::Screen screen_ = ui::Screen::MiiList;
    bool running_ = true;
};

}  // namespace mii::app
