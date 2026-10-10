#include "app/mii_editor_application.hpp"
#include "platform/wii_platform.hpp"
#include "ui/ui_renderer.hpp"

int main() {
    mii::platform::WiiPlatform platform;
    platform.initialize();

    mii::ui::UiRenderer renderer;
    platform.configureGraphics();
    mii::app::MiiEditorApplication application(platform, renderer);
    application.run();
    return 0;
}
