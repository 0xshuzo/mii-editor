#include "ui/mii_editor_ui.hpp"

#include <array>
#include <cstddef>

#include "ui/ui_theme.hpp"

namespace mii::ui {
namespace {

using namespace theme;

struct Layout {
    static constexpr Rect frame{16, 12, 822, 456};
    static constexpr Rect header{16, 12, 822, 34};
    static constexpr Rect list{26, 56, 270, 360};
    static constexpr Rect details{306, 56, 522, 360};
    static constexpr Rect portrait{318, 90, 260, 314};
    static constexpr Rect currentCard{590, 98, 226, 82};
    static constexpr Rect currentField{602, 130, 202, 36};
    static constexpr Rect newCard{590, 190, 226, 80};
    static constexpr Rect newField{602, 222, 202, 36};
    static constexpr Rect infoCard{590, 284, 226, 98};
    static constexpr Rect footer{16, 428, 822, 38};
    static constexpr Rect modal{116, 40, 622, 424};
    static constexpr Rect title{128, 54, 598, 25};
    static constexpr Rect subtitle{128, 82, 598, 18};
    static constexpr Rect editorField{249, 108, 356, 46};
    static constexpr Rect tabs{128, 164, 598, 40};
    static constexpr Rect keyboard{128, 210, 598, 210};
    static constexpr Rect cancel{132, 432, 98, 28};
};

enum class KeyIcon { None, Backspace, Shift, Confirm };
struct Key { const char* label; float units; KeyIcon icon = KeyIcon::None; bool primary = false; };
struct KeyRow { const Key* keys; std::size_t count; float indent; };

constexpr Key kNumberRow[] = {{"1", 1}, {"2", 1}, {"3", 1}, {"4", 1}, {"5", 1}, {"6", 1},
                              {"7", 1}, {"8", 1}, {"9", 1}, {"0", 1}, {"-", 1}, {"=", 1},
                              {"Delete", 1.7F, KeyIcon::Backspace}};
constexpr Key kTopRow[] = {{"q", 1}, {"w", 1}, {"e", 1}, {"r", 1}, {"t", 1}, {"y", 1},
                           {"u", 1}, {"i", 1}, {"o", 1}, {"p", 1}, {"[", 1}, {"]", 1}};
constexpr Key kMiddleRow[] = {{"a", 1}, {"s", 1}, {"d", 1}, {"f", 1}, {"g", 1}, {"h", 1},
                              {"j", 1}, {"k", 1}, {"l", 1}, {";", 1}, {"'", 1}, {"#", 1}};
constexpr Key kBottomRow[] = {{"Shift", 1.7F, KeyIcon::Shift}, {"z", 1}, {"x", 1}, {"c", 1}, {"v", 1},
                              {"b", 1}, {"n", 1}, {"m", 1}, {",", 1}, {".", 1}, {"/", 1}};
constexpr Key kActionRow[] = {{"@", 1}, {"#+=", 1.55F}, {"Space", 4.55F}, {"Done", 2.9F, KeyIcon::Confirm, true}};
constexpr KeyRow kRows[] = {{kNumberRow, std::size(kNumberRow), 0.0F},
                            {kTopRow, std::size(kTopRow), 0.0F},
                            {kMiddleRow, std::size(kMiddleRow), 0.32F},
                            {kBottomRow, std::size(kBottomRow), 0.0F},
                            {kActionRow, std::size(kActionRow), 0.0F}};
constexpr std::array<MkwSymbol, 8> kSymbols{{MkwSymbol::Star, MkwSymbol::WiimoteTwo,
    MkwSymbol::WiimoteA, MkwSymbol::WiimoteB, MkwSymbol::WiimotePlus, MkwSymbol::Coin,
    MkwSymbol::Balloon, MkwSymbol::Trophy}};

Rect inset(const Rect value, const float amount) {
    return {value.x + amount, value.y + amount, value.width - amount * 2.0F, value.height - amount * 2.0F};
}

Color alpha(Color color, const std::uint8_t value) {
    color.alpha = value;
    return color;
}

void panel(UiRenderer& renderer, const Rect bounds, const float radius = kPanelRadius,
           const Color top = kPanel, const Color bottom = kPanelBottom) {
    renderer.useSolidPrimitives();
    renderer.fillRoundedRectangle({bounds.x, bounds.y + 2.0F, bounds.width, bounds.height}, radius, kShadow);
    renderer.fillRoundedRectangle(bounds, radius, kBorder);
    const Rect content = inset(bounds, 1.0F);
    renderer.fillRoundedVerticalGradient(content, radius - 1.0F, top, bottom);
}

void field(UiRenderer& renderer, const Rect bounds, const bool focused) {
    renderer.useSolidPrimitives();
    if (focused) renderer.fillRoundedRectangle(inset(bounds, -3.0F), kControlRadius + 3.0F, kBlueGlow);
    renderer.fillRoundedRectangle(bounds, kControlRadius, focused ? kBlue : kBorder);
    renderer.fillRoundedRectangle(inset(bounds, focused ? 2.0F : 1.0F), focused ? kControlRadius - 2.0F : kControlRadius - 1.0F, kWhite);
}

void button(UiRenderer& renderer, const Rect bounds, const char* label, const bool primary, const bool hover = false) {
    renderer.useSolidPrimitives();
    if (hover) renderer.fillRoundedRectangle(inset(bounds, -2.0F), kControlRadius + 2.0F, kBlueGlow);
    renderer.fillRoundedRectangle({bounds.x, bounds.y + 1.5F, bounds.width, bounds.height}, kControlRadius, alpha(kShadow, 150));
    renderer.fillRoundedRectangle(bounds, kControlRadius, primary ? kBlue : kBorder);
    const Rect content = inset(bounds, 1.0F);
    if (primary) renderer.fillRoundedVerticalGradient(content, kControlRadius - 1.0F,
                                                       {91, 173, 255, 255}, kBlue);
    else renderer.fillRoundedRectangle(content, kControlRadius - 1.0F, kWhite);
    renderer.drawTextCentered(bounds, label, primary ? 14.0F : 13.0F, primary ? kWhite : kInk);
}

void appIcon(UiRenderer& renderer) {
    renderer.useSolidPrimitives();
    renderer.fillCircle({42, 29}, 12, {70, 52, 43, 255});
    renderer.fillCircle({42, 30}, 7.5F, {255, 225, 203, 255});
    renderer.fillCircle({39, 30}, 1, kInk);
    renderer.fillCircle({45, 30}, 1, kInk);
    renderer.fillRoundedRectangle({38, 35, 8, 2.5F}, 1.0F, kWhite);
}

void infoIcon(UiRenderer& renderer, const Point center) {
    renderer.useSolidPrimitives();
    renderer.fillCircle(center, 8.5F, kMuted);
    renderer.drawTextCentered({center.x - 5, center.y - 6, 10, 12}, "i", 12, kWhite);
}

void gear(UiRenderer& renderer, const Point center) {
    renderer.useSolidPrimitives();
    renderer.fillCircle(center, 7, kMuted);
    renderer.fillCircle(center, 2.5F, kPanel);
    renderer.fillRectangle({center.x - 1.5F, center.y - 10, 3, 4}, kMuted);
    renderer.fillRectangle({center.x - 1.5F, center.y + 6, 3, 4}, kMuted);
    renderer.fillRectangle({center.x - 10, center.y - 1.5F, 4, 3}, kMuted);
    renderer.fillRectangle({center.x + 6, center.y - 1.5F, 4, 3}, kMuted);
}

void badge(UiRenderer& renderer, const Rect bounds, const char* label) {
    renderer.useSolidPrimitives();
    renderer.fillCircle({bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}, bounds.height / 2, kBorder);
    renderer.fillCircle({bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}, bounds.height / 2 - 1, kWhite);
    renderer.drawTextCentered(bounds, label, 12, kInk);
}

void keyIcon(UiRenderer& renderer, const Rect box, const KeyIcon icon, const Color color) {
    renderer.useSolidPrimitives();
    const float middle = box.y + box.height / 2;
    if (icon == KeyIcon::Backspace) {
        renderer.fillRoundedRectangle({box.x + 8, middle - 7, 13, 14}, 2, color);
        renderer.fillTriangle({box.x + 3, middle}, {box.x + 8, middle - 7}, {box.x + 8, middle + 7}, color);
        renderer.fillRectangle({box.x + 12, middle - 0.7F, 5, 1.4F}, kWhite);
    } else if (icon == KeyIcon::Shift) {
        renderer.fillTriangle({box.x + 12, middle - 9}, {box.x + 5, middle - 1}, {box.x + 19, middle - 1}, color);
        renderer.fillRectangle({box.x + 9, middle - 1, 6, 8}, color);
    } else if (icon == KeyIcon::Confirm) {
        renderer.fillCircle({box.x + 13, middle}, 8, kWhite);
        renderer.drawTextCentered({box.x + 7, middle - 6, 12, 12}, "+", 12, kBlue);
    }
}

void drawRow(UiRenderer& renderer, const KeyRow& row, const std::size_t rowIndex,
             const bool pointerVisible, const Point pointer) {
    constexpr float padding = 8.0F;
    constexpr float gap = 4.0F;
    constexpr float height = 35.0F;
    float units = row.indent;
    for (std::size_t index = 0; index < row.count; ++index) units += row.keys[index].units;
    const float unit = (Layout::keyboard.width - padding * 2.0F - static_cast<float>(row.count - 1) * gap) / units;
    float x = Layout::keyboard.x + padding + row.indent * unit;
    const float y = Layout::keyboard.y + padding + static_cast<float>(rowIndex) * (height + gap);
    for (std::size_t index = 0; index < row.count; ++index) {
        const Key& key = row.keys[index];
        const Rect box{x, y, key.units * unit, height};
        const bool hover = pointerVisible && box.contains(pointer);
        if (key.primary) {
            button(renderer, box, key.label, true, hover);
        } else {
            if (hover) {
                renderer.useSolidPrimitives();
                renderer.fillRoundedRectangle(inset(box, -2), kControlRadius + 2, kBlueGlow);
                renderer.fillRoundedRectangle(box, kControlRadius, kBlue);
                renderer.fillRoundedRectangle(inset(box, 2), kControlRadius - 2, kBlueLight);
            } else {
                field(renderer, box, false);
            }
            keyIcon(renderer, box, key.icon, kMuted);
            const float iconSpace = key.icon == KeyIcon::None ? 0 : 13;
            renderer.drawTextCentered({box.x + iconSpace, box.y, box.width - iconSpace, box.height},
                                      key.label, key.label[1] == '\0' ? 17.0F : 13.0F, kInk);
        }
        if (key.primary) keyIcon(renderer, box, key.icon, kBlue);
        x += box.width + gap;
    }
}

ArtworkId avatar(const std::size_t index) {
    return static_cast<ArtworkId>(index + 1);
}

}  // namespace

MiiEditorUi::MiiEditorUi() {
    artwork_.initialize();
}

Rect MiiEditorUi::miiNameField() const {
    return Layout::newField;
}

void MiiEditorUi::render(UiRenderer& renderer, const model::PlaceholderMiiRepository& miis,
                         const Screen screen, const bool hasPointer, const Point pointer) const {
    if (screen == Screen::RendererDiagnostics) {
        renderDiagnostics(renderer);
        return;
    }
    cursor_.update(hasPointer, pointer);
    renderMain(renderer, miis, hasPointer, pointer);
    if (screen == Screen::NameEditor) {
        renderer.useSolidPrimitives();
        renderer.fillRectangle({0, 0, kDesignWidth, kDesignHeight}, kScrim);
        renderEditor(renderer, miis.selected(), hasPointer, pointer);
    }
    renderPointer(renderer);
}

void MiiEditorUi::renderDiagnostics(UiRenderer& renderer) const {
    // Development-only baseline for isolating shared renderer coverage.  It
    // intentionally contains no text, art, cursor, or shadow layers.
    renderer.useSolidPrimitives();
    renderer.fillRectangle({0, 0, kDesignWidth, kDesignHeight}, {28, 35, 49, 255});
    renderer.fillRectangle({0, 0, kDesignWidth, 240}, {224, 91, 91, 255});
    renderer.fillRectangle({0, 240, kDesignWidth, 240}, {61, 168, 123, 255});
    renderer.fillRectangle({88, 70, 150, 54}, {247, 242, 104, 255});
    renderer.fillRoundedRectangle({286, 70, 150, 54}, 12, {249, 249, 255, 255});
    renderer.fillRoundedRectangle({88, 164, 348, 48}, 8, {255, 255, 255, 255});
    renderer.fillRoundedRectangle({88, 244, 52, 35}, 7, {255, 255, 255, 255});
    renderer.fillRoundedVerticalGradient({488, 70, 160, 48}, 8,
                                         {91, 173, 255, 255}, kBlue);
    renderer.fillRoundedVerticalGradient({488, 164, 240, 120}, 12,
                                         {244, 250, 255, 160}, {205, 229, 249, 160});
}

void MiiEditorUi::renderMain(UiRenderer& renderer, const model::PlaceholderMiiRepository& miis,
                             const bool hasPointer, const Point pointer) const {
    renderer.useSolidPrimitives();
    renderer.fillVerticalGradient({0, 0, kDesignWidth, kDesignHeight}, {205, 230, 248, 255}, kBackground);
    renderer.fillCircle({-24, 442}, 120, {230, 243, 253, 180});
    renderer.fillCircle({850, 86}, 106, {230, 243, 253, 160});
    panel(renderer, Layout::frame, 14, {250, 253, 255, 255}, {237, 247, 254, 255});
    panel(renderer, Layout::header, 14, kPanel, {244, 249, 254, 255});
    panel(renderer, Layout::list);
    panel(renderer, Layout::details);
    panel(renderer, Layout::footer, 12);

    appIcon(renderer);
    renderer.drawText({62, 17}, "Mii Editor", 22, kInk);
    renderer.drawText({166, 22}, "Edit Mii names", 13, kMuted);
    renderer.drawText({38, 64}, "My Miis", 18, kInk);
    renderer.drawText({245, 67}, "6 / 12", 13, kMuted);
    renderer.drawText({320, 64}, "Mii Details", 18, kInk);
    renderer.useSolidPrimitives();
    renderer.fillRectangle({318, 84, 498, 1}, kBorder);

    const auto& entries = miis.entries();
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const Rect row{36, 90 + static_cast<float>(index) * 54, 240, 48};
        if (index == 1) {
            renderer.useSolidPrimitives();
            renderer.fillRoundedRectangle(inset(row, -3), 11, kBlueGlow);
            renderer.fillRoundedRectangle(row, 9, kBlue);
            renderer.fillRoundedRectangle(inset(row, 2), 7, kBlueLight);
        } else {
            field(renderer, row, false);
        }
        const Rect thumbnail{44, row.y + 3, 42, 42};
        renderer.pushClip(thumbnail);
        artwork_.draw(renderer, avatar(entries[index].artworkIndex), thumbnail);
        renderer.popClip();
        renderer.drawText({100, row.y + 12}, entries[index].displayName, 18, kInk);
    }
    renderer.useSolidPrimitives();
    renderer.fillRoundedRectangle({283, 91, 5, 310}, 2.5F, {221, 229, 239, 255});
    renderer.fillRoundedRectangle({283, 93, 5, 112}, 2.5F, {163, 179, 199, 255});

    renderer.pushClip(Layout::portrait);
    artwork_.draw(renderer, ArtworkId::HeroSora, Layout::portrait);
    renderer.popClip();
    panel(renderer, Layout::currentCard, 10, kPanelBlue, {224, 239, 252, 255});
    renderer.drawText({602, 108}, "Mii Name", 14, kMuted);
    field(renderer, Layout::currentField, false);
    renderer.drawText({616, 137}, miis.selected().displayName, 20, kInk);
    panel(renderer, Layout::newCard, 10, kPanelBlue, {224, 239, 252, 255});
    renderer.drawText({602, 200}, "New Name", 14, kMuted);
    field(renderer, Layout::newField, true);
    renderer.drawText({616, 229}, miis.selected().displayName, 20, kInk);
    panel(renderer, Layout::infoCard, 10, kPanelBlue, {227, 241, 253, 255});
    infoIcon(renderer, {608, 303});
    renderer.drawText({624, 294}, "Info", 14, kMuted);
    renderer.drawText({602, 321}, "Change this Mii's name", 12, kMuted);
    renderer.drawText({602, 340}, "before using it in game.", 12, kMuted);

    gear(renderer, {45, 447});
    renderer.drawText({60, 440}, "Settings", 13, kMuted);
    const Rect mainCancel{694, 433, 62, 28};
    const Rect save{766, 433, 60, 28};
    button(renderer, mainCancel, "Cancel", false, hasPointer && mainCancel.contains(pointer));
    button(renderer, save, "Save", true, hasPointer && save.contains(pointer));
}

void MiiEditorUi::renderEditor(UiRenderer& renderer, const model::PlaceholderMii& mii,
                               const bool hasPointer, const Point pointer) const {
    panel(renderer, Layout::modal, 14, {252, 254, 255, 255}, {240, 248, 255, 255});
    renderer.drawTextCentered(Layout::title, "Edit Mii Name", 22, kInk);
    renderer.drawTextCentered(Layout::subtitle, "Enter a new name for this Mii.", 13, kMuted);
    field(renderer, Layout::editorField, true);
    const float nameX = Layout::editorField.x + 18;
    renderer.drawText({nameX, Layout::editorField.y + 8}, mii.displayName, 24, kInk);
    renderer.useSolidPrimitives();
    renderer.fillRectangle({nameX + renderer.textWidth(mii.displayName, 24) + 3,
                            Layout::editorField.y + 11, 2, 25}, kBlue);

    panel(renderer, Layout::tabs, 10, kPanelBlue, {222, 237, 251, 255});
    button(renderer, {136, 171, 122, 26}, "Letters", true);
    button(renderer, {264, 171, 98, 26}, "Symbols", false);
    for (std::size_t index = 0; index < kSymbols.size(); ++index) {
        const Rect box{370 + static_cast<float>(index) * 43, 171, 37, 26};
        field(renderer, box, false);
        renderer.drawMkwSymbol(inset(box, 5), kSymbols[index], kInk);
    }

    panel(renderer, Layout::keyboard, 10, kPanelBlue, {218, 234, 249, 255});
    for (std::size_t row = 0; row < std::size(kRows); ++row)
        drawRow(renderer, kRows[row], row, hasPointer, pointer);
    button(renderer, Layout::cancel, "Cancel", false, hasPointer && Layout::cancel.contains(pointer));
    renderer.useSolidPrimitives();
    renderer.fillRectangle({352, 437, 1, 18}, kBorder);
    badge(renderer, {382, 436, 20, 20}, "A");
    renderer.drawText({408, 439}, "Select", 12, kMuted);
    badge(renderer, {475, 436, 20, 20}, "B");
    renderer.drawText({501, 439}, "Delete", 12, kMuted);
    badge(renderer, {567, 436, 20, 20}, "2");
    renderer.drawText({593, 439}, "Space", 12, kMuted);
    badge(renderer, {647, 436, 20, 20}, "+");
    renderer.drawText({673, 439}, "Done", 12, kMuted);
}

void MiiEditorUi::renderPointer(UiRenderer& renderer) const {
    cursor_.render(renderer);
}

}  // namespace mii::ui
