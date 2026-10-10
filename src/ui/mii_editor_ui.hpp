#pragma once

#include "model/placeholder_mii.hpp"
#include "ui/ui_assets.hpp"
#include "ui/ui_renderer.hpp"
#include "ui/wii_cursor.hpp"

namespace mii::ui {

enum class Screen {
    MiiList,
    NameEditor,
    // Selected only by a compile-time development setting.  It does not add
    // navigation or application behaviour to the presentation build.
    RendererDiagnostics,
};

// Stateless view of the application. Screen transitions and future Mii editing
// are intentionally owned by MiiEditorApplication.
class MiiEditorUi {
public:
    MiiEditorUi();

    [[nodiscard]] Rect miiNameField() const;

    void render(UiRenderer& renderer, const model::PlaceholderMiiRepository& miis, Screen screen,
                bool hasPointer, Point pointer) const;

private:
    void renderMain(UiRenderer& renderer, const model::PlaceholderMiiRepository& miis,
                    bool hasPointer, Point pointer) const;
    void renderEditor(UiRenderer& renderer, const model::PlaceholderMii& mii,
                      bool hasPointer, Point pointer) const;
    void renderDiagnostics(UiRenderer& renderer) const;
    void renderPointer(UiRenderer& renderer) const;

    mutable WiiCursor cursor_{};
    UiAssets artwork_{};
};

}  // namespace mii::ui
