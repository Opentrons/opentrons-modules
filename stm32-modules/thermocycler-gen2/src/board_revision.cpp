
// My header
#include "thermocycler-gen2/board_revision.hpp"

#include "thermocycler-gen2/board_revision_hardware.h"
// Library headers
#include <array>
#include <optional>

using namespace board_revision;

// Holds the expected inputs for a board rev
struct BoardRevSetting {
    const std::array<TrinaryInput_t, BOARD_REV_PIN_COUNT> pins;
    const BoardRevision revision;
};

// Expected GPIO inputs for each board rev
constexpr static std::array<BoardRevSetting, BOARD_REV_INVALID> _revisions{
    {{.pins = {INPUT_FLOATING, INPUT_FLOATING, INPUT_FLOATING},
      .revision = BOARD_REV_1},
     {.pins = {INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN},
      .revision = BOARD_REV_2}}};
// The actual revision
static auto _revision = BoardRevision::BOARD_REV_INVALID;

auto BoardRevisionIface::get() -> BoardRevision {
    if (_revision == BoardRevision::BOARD_REV_INVALID) {
        read();
    }
    return _revision;
}

auto BoardRevisionIface::read() -> BoardRevision {
    auto inputs = std::array<TrinaryInput_t, BOARD_REV_PIN_COUNT>();
    board_revision_read_inputs(inputs.data());
    for (auto setting : _revisions) {
        if ((inputs.at(0) == setting.pins.at(0)) &&
            (inputs.at(1) == setting.pins.at(1)) &&
            (inputs.at(2) == setting.pins.at(2))) {
            _revision = setting.revision;
            return _revision;
        }
    }
    _revision = BoardRevision::BOARD_REV_INVALID;
    return _revision;
}
