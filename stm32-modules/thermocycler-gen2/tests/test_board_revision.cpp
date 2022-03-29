#include "catch2/catch.hpp"
#include "test/test_board_revision_hardware.hpp"
#include "thermocycler-gen2/board_revision.hpp"

SCENARIO("board revision checking works") {
    std::array<TrinaryInput_t, BOARD_REV_PIN_COUNT> inputs = {
        INPUT_FLOATING, INPUT_FLOATING, INPUT_FLOATING};
    GIVEN("all pins floating") {
        board_revision::set_pin_values(inputs);
        WHEN("getting board revision") {
            auto revision = board_revision::BoardRevisionIface::get();
            THEN("the revision is rev1") {
                REQUIRE(revision == board_revision::BoardRevision::BOARD_REV_1);
            }
        }
        WHEN("reading board revision") {
            auto revision = board_revision::BoardRevisionIface::read();
            THEN("the revision is rev1") {
                REQUIRE(revision == board_revision::BoardRevision::BOARD_REV_1);
            }
        }
    }
    GIVEN("all pins pulled down") {
        inputs.at(0) = INPUT_PULLDOWN;
        inputs.at(1) = INPUT_PULLDOWN;
        inputs.at(2) = INPUT_PULLDOWN;
        board_revision::set_pin_values(inputs);
        WHEN("getting board revision without rereading") {
            auto revision = board_revision::BoardRevisionIface::get();
            THEN("the revision is still rev1") {
                REQUIRE(revision == board_revision::BoardRevision::BOARD_REV_1);
            }
        }
        WHEN("reading board revision") {
            auto revision = board_revision::BoardRevisionIface::read();
            THEN("the revision is rev2") {
                REQUIRE(revision == board_revision::BoardRevision::BOARD_REV_2);
            }
        }
    }
    GIVEN("one pin pulled down and two pulled up") {
        inputs.at(0) = INPUT_PULLDOWN;
        inputs.at(1) = INPUT_PULLUP;
        inputs.at(2) = INPUT_PULLUP;
        board_revision::set_pin_values(inputs);
        WHEN("getting board revision without rereading") {
            auto revision = board_revision::BoardRevisionIface::get();
            THEN("the revision is rev1") {
                REQUIRE(revision == board_revision::BoardRevision::BOARD_REV_2);
            }
        }
        WHEN("reading board revision") {
            auto revision = board_revision::BoardRevisionIface::read();
            THEN("the revision is invalid") {
                REQUIRE(revision ==
                        board_revision::BoardRevision::BOARD_REV_INVALID);
            }
        }
    }
}
