#include "model/placeholder_mii.hpp"

#include <cstddef>

namespace mii::model {
namespace {

constexpr std::array<PlaceholderMii, 6> kPlaceholderMiis{{
    {"Luca", 0},
    {"Sora", 1},
    {"Mika", 2},
    {"Rex", 3},
    {"Nina", 4},
    {"Tobi", 5},
}};

}  // namespace

const std::array<PlaceholderMii, 6>& PlaceholderMiiRepository::entries() const {
    return kPlaceholderMiis;
}

const PlaceholderMii& PlaceholderMiiRepository::selected() const {
    return kPlaceholderMiis[static_cast<std::size_t>(selectedIndex_)];
}

void PlaceholderMiiRepository::select(const int index) {
    // TODO: Validate and persist a selected Mii once the Mii data source exists.
    (void)index;
}

}  // namespace mii::model
