#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "firmware/tmf8820_image.h"
#include "firmware/tmf8820_spadmap.h"
#include "systemwide.h"
#include "tmf8820_registers.hpp"
#include "tmf8820_spadmaps.hpp"
#include "tof_utils.hpp"

namespace tmf8820 {
using namespace tof::hardware;
using namespace tmf8820_spad;

template <typename P>
concept TMF8820Policy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

// I2C Device address
constexpr uint16_t TOF_DEFAULT_ADDRESS = 0x41;
constexpr uint16_t TOF_X_ADDRESS = 0x39;
constexpr uint16_t TOF_Z_ADDRESS = 0x40;
// Frame retry defaults
constexpr uint8_t DEFAULT_RETRIES = 5;
constexpr uint32_t DEFAULT_SLEEP_MS = 250;

// Default config
constexpr uint8_t DEFAULT_SPAD_MAP_ID = 14;
constexpr uint16_t DEFAULT_REPORT_PERIOD_MS = 500;
constexpr uint16_t DEFAULT_KILO_ITERATIONS = 4000;
constexpr bool DEFAULT_HISTOGRAM_DUMP = true;

// Bootloader commands (bl_cmd_stat)

// Remap RAM to Address 0 and Reset
constexpr uint8_t BL_RAMREMAP_RESET = 0x11;
// Initialize for RAM download from host to TMF8820/21/28
constexpr uint8_t BL_DOWNLOAD_INIT = 0x14;
// Build in self-test of RAM (pattern test)
constexpr uint8_t BL_RAM_BIST = 0x2A;
// Build in self-test of I²C RAM (pattern test)
constexpr uint8_t BL_I2C_BIST = 0x2C;
// Write RAM Region (Plain = not encoded into e.g. Intel Hex Records)
constexpr uint8_t BL_W_RAM = 0x41;
// Set the read/write RAM pointer to a given address
constexpr uint8_t BL_ADDR_RAM = 0x43;

// Bootloader cmd status
enum BL_CMD_STATUS : uint8_t {
    STAT_READY = 0,
    STAT_ERR_SIZE,
    STAT_ERR_CSUM,
    STAT_ERR_RANGE,
    STAT_ERR_MORE,
};

// Bootloader Payload constants
constexpr uint8_t BL_DATA_LEN = 128;  // chunk len
constexpr uint8_t BL_HEADER_LEN = 2;  // cmd + len
constexpr uint8_t BL_FOOTER_LEN = 1;  // checksum
constexpr uint8_t BUFFER_LEN = 150;

// CMD_STAT Commands
constexpr uint8_t CMD_MEASURE = 0x10;
constexpr uint8_t CMD_WRITE_CONFIG_PAGE = 0x15;
constexpr uint8_t CMD_LOAD_CONFIG_PAGE_COMMON = 0x16;
constexpr uint8_t CMD_LOAD_CONFIG_PAGE_SPAD_1 = 0x17;
constexpr uint8_t CMD_LOAD_CONFIG_PAGE_SPAD_2 = 0x18;
constexpr uint8_t CMD_LOAD_CONFIG_PAGE_FACTORY_CALIB = 0x19;
constexpr uint8_t CMD_FACTORY_CALIBRATION = 0x20;
constexpr uint8_t CMD_I2C_SLAVE_ADDRESS = 0x21;
constexpr uint8_t CMD_RESET = 0xFE;
constexpr uint8_t CMD_STOP = 0xFF;

// CMD_STAT Results
constexpr uint8_t STAT_OK = 0x00;
constexpr uint8_t STAT_ACCEPTED = 0x01;
constexpr uint8_t STAT_ERR_CONFIG = 0x02;
constexpr uint8_t STAT_ERR_APPLICATION = 0x03;
constexpr uint8_t STAT_ERR_WAKEUP_TIMED = 0x04;
constexpr uint8_t STAT_ERR_RESET_UNEXPECTED = 0x05;
constexpr uint8_t STAT_ERR_UNKNOWN_CMD = 0x06;
constexpr uint8_t STAT_ERR_NO_REF_SPAD = 0x07;
constexpr uint8_t STAT_ERR_UNKNOWN_CID = 0x09;
constexpr uint8_t STAT_WARNING_CONFIG_SPAD_1_NOT_ACCEPTED = 0x0A;
constexpr uint8_t STAT_WARNING_CONFIG_SPAD_2_NOT_ACCEPTED = 0x0B;
constexpr uint8_t STAT_WARNING_OSC_TRIM_NOT_ACCEPTED = 0x0C;
constexpr uint8_t STAT_WARNING_I2C_ADDRESS_NOT_ACCEPTED = 0x0D;
constexpr uint8_t STAT_ERR_UNKNOWN_MODE = 0x0E;

// SPAD Map Config

// 3x3 normal mode 33°x32° FoV
constexpr uint8_t SPAD_MAP_ID_1 = 1;
// 3x3 macro 1 mode 33°x47° FoV off center
constexpr uint8_t SPAD_MAP_ID_2 = 2;
// 3x3 macro 2 mode 33°x47° FoV
constexpr uint8_t SPAD_MAP_ID_3 = 3;
// 3x3 wide mode 41°x52° FoV
constexpr uint8_t SPAD_MAP_ID_6 = 6;
// 3x3 mode 33°x32° FoV, checkerboard
constexpr uint8_t SPAD_MAP_ID_11 = 11;
// 3x3 mode 33°x32° FoV, inverted checkerboard
constexpr uint8_t SPAD_MAP_ID_12 = 12;
// User defined mode, single measurement mode
// using config_page_spad1 only
constexpr uint8_t SPAD_MAP_ID_14 = 14;

// Histogram measurement
// 3 header + 4 sub header + 128bytes = 135bytes
constexpr uint8_t HIST_FRAME_LEN = 135;
constexpr uint8_t HIST_DATA_LEN = HIST_FRAME_LEN - 3 - 4;
// Number of histogram frames
constexpr uint8_t HIST_DATA_COUNT = 30;
// NOLINTNEXTLINE
std::array<uint8_t, HIST_FRAME_LEN> hist_buffer = {0};

constexpr uint8_t HISTOGRAM_REG = 0x20;
constexpr uint8_t HISTOGRAM_DELIM = 0x81;
constexpr uint8_t HIST_LAST_COUNT = 0x1D;  // 0..29, 30 packets
constexpr uint8_t HIST_DATA_START = 7;     // byte where histogram data starts
constexpr uint8_t HIST_CLEAR_IRQ = 0xFF;

// Measurement status
constexpr uint8_t HIST_OK = 0;
constexpr uint8_t HIST_NOT_READY = 1;
constexpr uint8_t HIST_DONE = 2;
constexpr uint8_t HIST_ERROR = 3;

// Active range
enum TOFActiveRange : uint8_t {
    NOT_SUPPORTED = 0,
    SHORT_RANGE = 0x6E,
    LONG_RANGE = 0x6F,
};

struct TMF8820Config {
    tmf8820::TMF8820RegisterMap* registers;
    const tmf8820_spad::TMF8820SPADConfig* spad_config;
    TOFActiveRange active_range = SHORT_RANGE;
    uint16_t report_period_ms = DEFAULT_REPORT_PERIOD_MS;
    uint16_t kilo_iterations = DEFAULT_KILO_ITERATIONS;
    bool histogram_dump = DEFAULT_HISTOGRAM_DUMP;
    uint8_t spad_map_id = DEFAULT_SPAD_MAP_ID;
};

class TMF8820 {
  public:
    auto initialize(TMF8820Config* config, TOFSensorPolicy* policy,
                    TOFSensorID sensor_id) -> bool {
        if (_policy == nullptr) {
            _policy = policy;
        }

        _config = config;
        // Need to wait when you toggle the init pin.
        TOFSensorPolicy::sleep_ms(DEFAULT_SLEEP_MS);
        TOFSensorPolicy::enable_tof_sensor(sensor_id, true);
        // Wait for the sensor to start
        TOFSensorPolicy::sleep_ms(DEFAULT_SLEEP_MS * 2);

        // Make sure the sensor is ready
        if (!set_sensor_ready(sensor_id)) {
            return false;
        }

        // Load the image from flash if the sensor is in bootloader mode
        if (get_sensor_mode(sensor_id) == TOFSensorMode::BOOTLOADER) {
            if (!handle_bootloader(sensor_id)) {
                return false;
            }
        }

        // Get the custom address this device should have
        auto address = get_custom_i2c_address(sensor_id);
        if (!set_sensor_i2c_address(sensor_id, address)) {
            // NOLINTNEXTLINE(readability-simplify-boolean-expr)
            return false;
        }

        // Set sensor configuration
        if (!configure_sensor(sensor_id, config)) {
            return false;
        }

        // TODO: Load calibration
        return true;
    }

    auto update_enable(TOFSensorID sensor_id) -> bool {
        auto reg = _config->registers->enable;
        reg.padding_1 = 0;
        return set_register(reg, sensor_id).has_value();
    }

    auto write(TOFSensorID sensor_id, uint16_t reg, uint8_t* data,
               uint size = 1) -> std::optional<RegisterSerializedType> {
        auto dev_address = get_sensor_i2c_address(sensor_id);
        auto res = _policy->i2c_write(dev_address << 1, reg, data, size);
        if (res != 0) {
            return {};
        }
        return {res};
    }

    auto read(TOFSensorID sensor_id, uint16_t reg, int size = 1)
        -> std::optional<RegisterSerializedType> {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        std::array<uint8_t, 5> data = {0};
        auto dev_address = get_sensor_i2c_address(sensor_id);
        auto res = _policy->i2c_read(dev_address << 1, reg, data.data(), size);
        if (res != 0) {
            return {};
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        auto value = static_cast<uint32_t>(*data.data());
        return {value};
    }

    /* Check which app (bootloader or measurement ) is running. */
    auto get_sensor_mode(TOFSensorID sensor_id) -> TOFSensorMode {
        auto ret = read_register<tmf8820::AppID>(sensor_id);
        if (!ret.has_value()) {
            return TOFSensorMode::UNKNOWN;
        }
        auto appid = static_cast<tmf8820::AppID>(ret.value()).appid;
        return static_cast<TOFSensorMode>(appid);
    }

    /* Check what active range mode is running. */
    auto get_sensor_active_range(TOFSensorID sensor_id) -> TOFActiveRange {
        auto ret = read_register<tmf8820::ActiveRange>(sensor_id);
        if (!ret.has_value()) {
            return TOFActiveRange::NOT_SUPPORTED;
        }
        auto mode = static_cast<tmf8820::ActiveRange>(ret.value()).active_range;
        return static_cast<TOFActiveRange>(mode);
    }

    auto reset_custom_address() -> void { _custom_address = false; }

    // Gets the sensor i2c address
    // NOLINTNEXTLINE(readability-make-member-function-const)
    auto get_sensor_i2c_address(TOFSensorID sensor_id) -> uint16_t {
        if (!_custom_address) {
            return TOF_DEFAULT_ADDRESS;
        }
        return get_custom_i2c_address(sensor_id);
    }

    auto set_sensor_i2c_address(TOFSensorID sensor_id, uint8_t address)
        -> bool {
        // switch to the appid=0x03, cid_rid=0x16 – Configuration Page
        if (!change_config_page(sensor_id, CMD_LOAD_CONFIG_PAGE_COMMON)) {
            return false;
        }

        // Set the new address in the I2C_SLAVE_ADDRESS (0x3B) register.
        _config->registers->i2c_address.slave_address = address;
        if (!set_register(_config->registers->i2c_address, sensor_id)
                 .has_value()) {
            return false;
        }

        // Set change address I2C_ADDR_CHANGE (0x3E) to 0
        _config->registers->i2c_addr_change = {0};
        if (!set_register(_config->registers->i2c_addr_change, sensor_id)
                 .has_value()) {
            return false;
        }

        // Write the config page
        if (!send_write_config_page(sensor_id)) {
            return false;
        }

        // Apply the new address with CMD_I2C_SLAVE_ADDRESS to CMD_STAT (0x08)
        // reg. Need to wait for registers to change.
        TOFSensorPolicy::sleep_ms(DEFAULT_SLEEP_MS * 2);
        auto len = prepare_cmd_frame(CMD_I2C_SLAVE_ADDRESS, nullptr, 0);
        if (!write(sensor_id, CMDStat::address, BUFFER.data(), len)
                 .has_value()) {
            return false;
        }

        // Use the new i2c address to verify comms.
        _custom_address = true;
        return wait_for_state(sensor_id, CMDStat::address, STAT_OK);
    }

    auto set_sensor_spad_map(TOFSensorID sensor_id, uint8_t spad_map_id)
        -> bool {
        if (!validate_spad_map_id(spad_map_id)) {
            return false;
        }

        // Switch to the appid=0x03, cid_rid=0x16 – Configuration Page
        if (!change_config_page(sensor_id, CMD_LOAD_CONFIG_PAGE_COMMON)) {
            return false;
        }

        // Set the SPAD_MAP_ID Register (0x34)
        _config->registers->spad_map_id.spad_map_id = spad_map_id;
        if (!set_register(_config->registers->spad_map_id, sensor_id)
                 .has_value()) {
            return false;
        }
        // Write the config page
        if (!send_write_config_page(sensor_id)) {
            return false;
        }

        // Set custom spad map if spad map id is 14
        if (spad_map_id == SPAD_MAP_ID_14) {
            if (!set_custom_spad_map(sensor_id, spad_map_id)) {
                return false;
            }
        }
        return true;
    }

    auto set_custom_spad_map(TOFSensorID sensor_id, uint8_t spad_map_id)
        -> bool {
        if (spad_map_id != SPAD_MAP_ID_14) {
            return false;
        }
        // Switch to the appid=0x03, cid_rid=0x17 – SPAD configuration
        if (!change_config_page(sensor_id, CMD_LOAD_CONFIG_PAGE_SPAD_1)) {
            return false;
        }

        // Load the custom SPAD mask
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        const auto* spad_mask = sensor_id == TOF_X ? spad_mask_x : spad_mask_z;
        auto spad_mask_len =
            sensor_id == TOF_X ? spad_mask_x_length : spad_mask_z_length;
        std::memcpy(BUFFER.data(), spad_mask, spad_mask_len);

        // Write the packed SPAD mask to SPAD_ENABLE_FIRST (0x24) Register
        if (!write(sensor_id, SPADEnable::address, BUFFER.data(), spad_mask_len)
                 .has_value()) {
            return false;
        }

        // Load the custom SPAD map
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        const auto* spad_map = sensor_id == TOF_X ? spad_map_x : spad_map_z;
        const uint spad_map_len =
            sensor_id == TOF_X ? spad_map_x_length : spad_map_z_length;
        std::memcpy(BUFFER.data(), spad_map, spad_map_len);
        // Write the packed SPAD map to SPAD_TDC_FIRST (0x42) Register
        if (!write(sensor_id, SPADTDCChannel::address, BUFFER.data(),
                   spad_map_len)
                 .has_value()) {
            return false;
        }

        // configure spad offset (0x8D, 0x8E)
        _config->registers->spad_offset = {_config->spad_config->xoff_q1,
                                           _config->spad_config->yoff_q1};
        if (!set_register(_config->registers->spad_offset, sensor_id)
                 .has_value()) {
            return false;
        }
        // configure spad size x (0x8F)
        _config->registers->spad_size_x.x_size = _config->spad_config->xsize;
        if (!set_register(_config->registers->spad_size_x, sensor_id)
                 .has_value()) {
            return false;
        }
        // configure spad size y (0x90)
        _config->registers->spad_size_y.y_size = _config->spad_config->ysize;
        if (!set_register(_config->registers->spad_size_y, sensor_id)
                 .has_value()) {
            return false;
        }
        // Write the config page
        if (!send_write_config_page(sensor_id)) {
            return false;
        }
        return true;
    }

    auto static validate_spad_map_id(uint8_t spad_map_id) -> bool {
        switch (spad_map_id) {
            case SPAD_MAP_ID_1:
            case SPAD_MAP_ID_2:
            case SPAD_MAP_ID_3:
            case SPAD_MAP_ID_6:
            case SPAD_MAP_ID_11:
            case SPAD_MAP_ID_12:
            case SPAD_MAP_ID_14:
                return true;
            default:
                return false;
        }
    }

    auto set_sensor_active_range(TOFSensorID sensor_id,
                                 TOFActiveRange active_range) -> bool {
        // switch to the appid=0x03, cid_rid=0x16 – Configuration Page
        if (!change_config_page(sensor_id, CMD_LOAD_CONFIG_PAGE_COMMON)) {
            return false;
        }
        if (active_range == NOT_SUPPORTED ||
            get_sensor_active_range(sensor_id) == NOT_SUPPORTED) {
            return false;
        }
        _config->registers->active_range.active_range = active_range;
        if (!set_register(_config->registers->active_range, sensor_id)
                 .has_value()) {
            return false;
        }
        // NOTE:
        // To change a register value you have to first change to the relevant
        // config page, write the value to that register, then issue a cmd write
        // 0x15 on the cmd_stat register (0x08) for that value to be applied.
        // Annoyingly, for the active range register you have write the active
        // range value (0x6E or 0x6F) to the cmd_stat register (0x08) instead
        // of issuing a write config 0x15 command to the 0x08 register.
        BUFFER[0] = active_range;
        if (!write(sensor_id, CMDStat::address, BUFFER.data(), 1).has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, CMDStat::address, STAT_OK);
    }

    auto set_sensor_histogram_dump(TOFSensorID sensor_id, bool enable) -> bool {
        // switch to the appid=0x03, cid_rid=0x16 – Configuration Page
        if (!change_config_page(sensor_id, CMD_LOAD_CONFIG_PAGE_COMMON)) {
            return false;
        }
        _config->registers->hist_dump.histogram = static_cast<uint8_t>(enable);
        if (!set_register(_config->registers->hist_dump, sensor_id)
                 .has_value()) {
            return false;
        }
        // Write the config page
        return send_write_config_page(sensor_id);
    }

    auto set_sensor_report_period(TOFSensorID sensor_id, uint16_t period_ms = 0)
        -> bool {
        // switch to the appid=0x03, cid_rid=0x16 – Configuration Page
        if (!change_config_page(sensor_id, CMD_LOAD_CONFIG_PAGE_COMMON)) {
            return false;
        }
        // update the period if given
        if (period_ms > 0) {
            _config->registers->report_period_ms = {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<uint8_t>(period_ms & 0xFF),  // lsb
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<uint8_t>((period_ms & 0xFF00) >> 8)};  // msb
        }
        if (!set_register(_config->registers->report_period_ms, sensor_id)
                 .has_value()) {
            return false;
        }
        // Write the config page
        return send_write_config_page(sensor_id);
    }

    auto set_sensor_kilo_iterations(TOFSensorID sensor_id,
                                    uint16_t iterations = 0) -> bool {
        // switch to the appid=0x03, cid_rid=0x16 – Configuration Page
        if (!change_config_page(sensor_id, CMD_LOAD_CONFIG_PAGE_COMMON)) {
            return false;
        }
        // update iterations if given
        if (iterations > 0) {
            _config->registers->kilo_iterations = {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<uint8_t>(iterations & 0xFF),  // lsb
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<uint8_t>((iterations & 0xFF00) >> 8)};  // msb
        }
        if (!set_register(_config->registers->kilo_iterations, sensor_id)
                 .has_value()) {
            return false;
        }
        // Write the config page
        return send_write_config_page(sensor_id);
    }

    auto start_measurement(TOFSensorID sensor_id,
                           TOFMeasurementKind kind = HISTOGRAM) -> int16_t {
        // Clear all interupts by writing 0xFF to INT_STATUS (0xE1)
        BUFFER[0] = HIST_CLEAR_IRQ;
        if (!write(sensor_id, INTStatus::address, BUFFER.data(), 1)
                 .has_value()) {
            return -1;
        }

        // Enable interrupts for histogram measurements
        _config->registers->int_enable.int4_enab = 1;
        if (!set_register(_config->registers->int_enable, sensor_id)
                 .has_value()) {
            return -1;
        }

        // Start a cyclic measurement according to the configuration
        auto len = prepare_cmd_frame(CMD_MEASURE, nullptr, 0);
        if (!write(sensor_id, CMDStat::address, BUFFER.data(), len)
                 .has_value()) {
            return -1;
        }
        if (!wait_for_state(sensor_id, CMDStat::address, STAT_ACCEPTED)) {
            return -1;
        }
        return get_total_measurement_length(kind);
    }

    auto stop_measurement(TOFSensorID sensor_id) -> bool {
        // Clear all interupts by writing 0xFF to INT_STATUS (0xE1)
        BUFFER[0] = HIST_CLEAR_IRQ;
        if (!write(sensor_id, INTStatus::address, BUFFER.data(), 1)
                 .has_value()) {
            return false;
        }

        // Disable interrupts
        _config->registers->int_enable.int2_enab = 0;
        _config->registers->int_enable.int4_enab = 0;
        _config->registers->int_enable.int6_enab = 0;
        _config->registers->int_enable.int7_enab = 0;
        if (!set_register(_config->registers->int_enable, sensor_id)
                 .has_value()) {
            return false;
        }

        // Send CMD_STOP (0xFF) to stop any ongoing measurements
        auto len = prepare_cmd_frame(CMD_STOP, nullptr, 0);
        if (!write(sensor_id, CMDStat::address, BUFFER.data(), len)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, CMDStat::address, STAT_OK);
    }

    template <std::size_t N>
    auto get_histogram_chunk(TOFSensorID sensor_id,
                             std::array<char, N>& encoded) -> uint8_t {
        // returns the next chunk of the histogram
        auto ret = read_register<tmf8820::INTStatus>(sensor_id);
        if (!ret.has_value()) {
            return HIST_ERROR;
        }
        auto reg = static_cast<tmf8820::INTStatus>(ret.value());
        if (!reg.int4) {
            return HIST_NOT_READY;
        }

        // hist ready, clear the int4 bit by writing `1` to it
        _config->registers->int_status.int4 = 1;
        if (!set_register(_config->registers->int_status, sensor_id)
                 .has_value()) {
            return HIST_ERROR;
        }

        // read the next histogram chunk
        auto dev_address = get_sensor_i2c_address(sensor_id);
        auto res = _policy->i2c_read(dev_address << 1, HISTOGRAM_REG,
                                     hist_buffer.data(), hist_buffer.size());
        if (res != 0) {
            return HIST_ERROR;
        }
        if (hist_buffer[0] != HISTOGRAM_DELIM) {
            return HIST_ERROR;
        }

        // encode the chunk as base64
        if (!base64_encode(hist_buffer, encoded)) {
            return HIST_ERROR;
        }
        // check if this is the last chunk
        return (hist_buffer[4] == HIST_LAST_COUNT) ? HIST_DONE : HIST_OK;
    }

  private:
    auto static get_custom_i2c_address(TOFSensorID sensor_id) -> uint16_t {
        switch (sensor_id) {
            case TOF_X:
                return TOF_X_ADDRESS;
            case TOF_Z:
                return TOF_Z_ADDRESS;
            default:
                return TOF_DEFAULT_ADDRESS;
        }
    }

    template <tmf8820::TMF8820Register Reg>
    requires WritableRegister<Reg>
    auto get_register_value(Reg reg) -> RegisterSerializedTypeA {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
        value &= Reg::value_mask;
        return value;
    }

    template <tmf8820::TMF8820Register Reg>
    requires ReadableRegister<Reg>
    auto read_register(TOFSensorID sensor_id) -> std::optional<Reg> {
        // Use the value_mask to compute the size in bytes to read.
        auto size = ((sizeof(Reg::value_mask) * ONE_BYTE) -
                     __builtin_clz(Reg::value_mask) + 7) /
                    ONE_BYTE;
        auto ret = read(sensor_id, Reg::address, size);
        if (!ret.has_value()) {
            return {};
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = *reinterpret_cast<Reg*>(&ret.value());
        return {value};
    }

    template <tmf8820::TMF8820Register Reg>
    requires WritableRegister<Reg>
    auto set_register(Reg reg, TOFSensorID sensor_id) -> std::optional<Reg> {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = get_register_value(reg);
        // Use the value_mask to compute the size in bytes to write.
        auto size = ((sizeof(Reg::value_mask) * ONE_BYTE) -
                     __builtin_clz(Reg::value_mask) + 7) /
                    ONE_BYTE;
        auto ret = write(sensor_id, Reg::address, (uint8_t*)&value, size);
        if (!ret.has_value()) {
            return {};
        }
        return {reg};
    }

    /* Makes sure the sensor is ready for communication */
    auto set_sensor_ready(TOFSensorID sensor_id) -> bool {
        auto ret = read_register<tmf8820::Enable>(sensor_id);
        if (!ret.has_value()) {
            return false;
        }
        auto reg = static_cast<tmf8820::Enable>(ret.value());
        if (!reg.pon || !reg.cpu_ready) {
            _config->registers->enable.pon = 1;
            _config->registers->enable.powerup_select = reg.powerup_select;
            update_enable(sensor_id);
            // Check if device is ready for comms
            for (uint8_t i = 0; i < DEFAULT_RETRIES; i++) {
                // Need to wait after setting Enable reg.
                TOFSensorPolicy::sleep_ms(DEFAULT_SLEEP_MS);
                ret = read_register<tmf8820::Enable>(sensor_id);
                if (ret.has_value()) {
                    reg = static_cast<tmf8820::Enable>(ret.value());
                    // device is ready for comms
                    if (reg.pon && reg.cpu_ready) {
                        return true;
                    }
                }
            }
            // Device is not ready for comms
            return false;
        }
        return true;
    }

    auto handle_bootloader(TOFSensorID sensor_id) -> bool {
        if (tmf8820_image_length < 1 || sensor_id == TOF_NONE) {
            return false;
        }

        // ready the driver for image download
        if (!bl_send_download_init(sensor_id)) {
            return false;
        }

        // set the target ram address
        if (!bl_send_set_address(sensor_id, tmf8820_image_start)) {
            return false;
        }

        // write firmware in 128 byte chunks to sensor
        uint8_t chunk_size = 0;
        for (uint32_t i = 0; i < tmf8820_image_length; i += BL_DATA_LEN) {
            chunk_size = (i + BL_DATA_LEN <= tmf8820_image_length)
                             ? BL_DATA_LEN
                             : (tmf8820_image_length - i);
            // send chunk
            if (!bl_send_image_chunk(sensor_id, &tmf8820_image[i],
                                     chunk_size)) {
                return false;
            }
        }

        // The image was downloaded successfully, jump to the measurement app.
        if (!bl_send_ram_remap_reset(sensor_id)) {
            return false;
        }

        // Verify that the measurement app is running.
        return wait_for_state(sensor_id, AppID::address, MEASURE);
    }

    auto static bl_create_checksum(const uint8_t* data, uint8_t len)
        -> uint8_t {
        uint8_t data_sum = 0;
        for (uint8_t i = 0; i < len; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            data_sum += data[i];
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        return data_sum ^ 0xFF;
    }

    // Formulate a bootloader frame in the BUFFER, returns the frame length.
    auto bl_prepare_cmd_frame(uint8_t cmd, const uint8_t* data, uint8_t len)
        -> uint8_t {
        if (len > BUFFER_LEN) {
            return 0;
        }
        auto data_len = BL_HEADER_LEN + len;
        // Add the header
        BUFFER[0] = cmd;
        BUFFER[1] = len;
        // Copy data to the buffer starting from the header len.
        for (int i = BL_HEADER_LEN; i < data_len; i++) {
            // NOLINTNEXTLINE
            BUFFER[i] = data[i - BL_HEADER_LEN];
        }
        // Add the checksum
        // NOLINTNEXTLINE
        BUFFER[data_len] = bl_create_checksum(BUFFER.data(), data_len);
        return data_len + BL_FOOTER_LEN;
    }

    // Send DOWNLOAD_INIT (0x14) to start the RAM download
    auto bl_send_download_init(TOFSensorID sensor_id) -> bool {
        std::array<uint8_t, 1> data = {tmf8820_image[0]};
        auto f_len = bl_prepare_cmd_frame(BL_DOWNLOAD_INIT, data.data(), 1);
        if (!write(sensor_id, BLStat::address, BUFFER.data(), f_len)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, BLStat::address, STAT_READY);
    }

    // Send ADDR_RAM (0x43) command to set RAM pointer to given address.
    auto bl_send_set_address(TOFSensorID sensor_id, uint16_t address) -> bool {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        std::array<uint8_t, 2> data = {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            static_cast<uint8_t>(address & 0xFF),  // lsb
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            static_cast<uint8_t>((address & 0xFF00) >> 8)};  // msb
        auto f_len = bl_prepare_cmd_frame(BL_ADDR_RAM, data.data(), 2);
        if (!write(sensor_id, BLStat::address, BUFFER.data(), f_len)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, BLStat::address, STAT_READY);
    }

    // Send W_RAM (0x41) command to write RAM region with the given data.
    auto bl_send_image_chunk(TOFSensorID sensor_id, const uint8_t* data,
                             uint8_t len) -> bool {
        auto f_len = bl_prepare_cmd_frame(BL_W_RAM, data, len);
        if (!write(sensor_id, BLStat::address, BUFFER.data(), f_len)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, BLStat::address, STAT_READY);
    }

    // Send RAMREMAP_RESET (0x11) command to jump to the application.
    auto bl_send_ram_remap_reset(TOFSensorID sensor_id) -> bool {
        auto f_len = bl_prepare_cmd_frame(BL_RAMREMAP_RESET, nullptr, 0);
        if (!write(sensor_id, BLStat::address, BUFFER.data(), f_len)) {
            return false;
        }

        // RAMREMAP_RESET jumps to the application if successful, BL commands
        // wont work anymore. Need to wait ~1s after resetting to let the
        // measure application start before checking CMD_STAT.
        TOFSensorPolicy::sleep_ms(DEFAULT_SLEEP_MS);
        return wait_for_state(sensor_id, CMDStat::address, STAT_OK);
    }

    // Formulate a CMD frame
    auto prepare_cmd_frame(uint8_t cmd, const uint8_t* data, uint8_t len)
        -> uint8_t {
        if (len > BUFFER_LEN) {
            return 0;
        }
        // Add the header
        BUFFER[0] = cmd;
        // Copy data to the buffer starting from the header len.
        for (uint8_t i = 1; i < len; i++) {
            // NOLINTNEXTLINE
            BUFFER[i] = data[i - 1];
        }
        return len + 1;
    }

    auto send_write_config_page(TOFSensorID sensor_id) -> bool {
        // Write the config page
        auto len = prepare_cmd_frame(CMD_WRITE_CONFIG_PAGE, nullptr, 0);
        if (!write(sensor_id, CMDStat::address, BUFFER.data(), len)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, CMDStat::address, STAT_OK);
    }

    // Changes the i2c page for commands.
    auto change_config_page(TOFSensorID sensor_id, uint8_t page) -> bool {
        BUFFER[0] = page;
        if (!write(sensor_id, CMDStat::address, BUFFER.data(), 1).has_value()) {
            return false;
        }
        auto stat_ok = wait_for_state(sensor_id, CMDStat::address, STAT_OK);
        // Verify page change
        if (stat_ok && wait_for_state(sensor_id, ConfigResult::address, page)) {
            _config_page = page;
            return true;
        }
        return false;
    }

    auto wait_for_state(TOFSensorID sensor_id, uint8_t reg, uint8_t value,
                        uint32_t sleep_ms = DEFAULT_SLEEP_MS,
                        uint8_t tries = DEFAULT_RETRIES) -> bool {
        while (tries > 0) {
            auto ret = read(sensor_id, reg);
            if (!ret.has_value()) {
                return false;
            }
            if (ret.value() == value) {
                return true;
            }
            tries -= 1;
            TOFSensorPolicy::sleep_ms(sleep_ms);
        }
        return false;
    }

    auto static get_total_measurement_length(TOFMeasurementKind kind)
        -> int16_t {
        switch (kind) {
            case HISTOGRAM:
                // Total bytes (128b * 30) for full histogram
                return HIST_DATA_LEN * HIST_DATA_COUNT;
            // Not implemented
            case MEASUREMENT:
            default:
                return -1;
        }
    }

    auto configure_sensor(TOFSensorID sensor_id, TMF8820Config* config)
        -> bool {
        if (!set_sensor_report_period(sensor_id, config->report_period_ms)) {
            return false;
        }
        if (!set_sensor_kilo_iterations(sensor_id, config->kilo_iterations)) {
            return false;
        }
        if (!set_sensor_active_range(sensor_id, config->active_range)) {
            return false;
        }
        if (!set_sensor_histogram_dump(sensor_id, config->histogram_dump)) {
            return false;
        }
        if (!set_sensor_spad_map(sensor_id, config->spad_map_id)) {
            return false;
        }
        return true;
    }

    TOFSensorPolicy* _policy{nullptr};
    TMF8820Config* _config{nullptr};
    std::array<uint8_t, BUFFER_LEN> BUFFER{};
    bool _custom_address{false};
    uint8_t _config_page{0};
};

}  // namespace tmf8820
