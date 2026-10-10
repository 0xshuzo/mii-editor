#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace mii::model {

// This is intentionally a presentation-only stand-in until Wii Mii data is wired in.
struct PlaceholderMii {
    std::string_view displayName;
    std::size_t artworkIndex = 0;
};

class PlaceholderMiiRepository {
public:
    static constexpr int kCapacity = 12;

    [[nodiscard]] const std::array<PlaceholderMii, 6>& entries() const;
    [[nodiscard]] const PlaceholderMii& selected() const;

    // TODO: Select an entry after real Mii storage has been introduced.
    void select(int index);

private:
    int selectedIndex_ = 1;
};

}  // namespace mii::model
