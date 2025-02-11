#pragma once

#include <cstdint>

#include "firmware/tmf8820_image.h"
#include "flex-stacker/tmf8821_registers.hpp"
#include "systemwide.h"

namespace tmf8821 {
using namespace tof::hardware;

template <typename P>
concept TMF8821Policy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

// I2C Device address
constexpr uint16_t TOF_DEFAULT_ADDRESS = 0x41;
constexpr uint16_t TOF_X_ADDRESS = 0x39;
constexpr uint16_t TOF_Z_ADDRESS = 0x40;
// Frame retry defaults
constexpr uint8_t DEFAULT_RETRIES = 5;
constexpr uint32_t DEFAULT_SLEEP_MS = 250;

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
enum BL_CMD_STATUS {
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
constexpr uint8_t BUFFER_LEN = 255;

// CMD_STAT Commands
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

class TMF8821 {
  public:
    auto initialize(const TMF8821RegisterMap& registers,
                    TOFSensorPolicy* policy, TOFSensorID sensor_id) -> bool {
        if (!_policy) {
            _policy = policy;
        }

        _registers = registers;

        // Need to wait when you toggle the init pin.
        _policy->sleep_ms(DEFAULT_SLEEP_MS);
        _policy->enable_tof_sensor(sensor_id, true);
        // Wait for the sensor to start
        _policy->sleep_ms(DEFAULT_SLEEP_MS * 2);

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
            return false;
        }

        //// TODO: Configure the spad maps
        // if (!configure_sensor(registers, sensor_id)) {
        //    return false;
        //}

        return true;
    }

    auto update_enable(const TMF8821RegisterMap& registers,
                       TOFSensorID sensor_id) -> bool {
        auto reg = registers.enable;
        reg.padding_1 = 0;
        return set_register(reg, sensor_id).has_value();
    }

    auto write(TOFSensorID sensor_id, uint16_t reg, uint8_t* data, int size = 1)
        -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        auto dev_address = get_sensor_i2c_address(sensor_id);
        auto [res, _] = _policy->i2c_write(dev_address << 1, reg, data, size);
        if (res != 0) {
            return RT();
        }
        return RT(res);
    }

    auto read(TOFSensorID sensor_id, uint16_t reg, int size = 1)
        -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        auto dev_address = get_sensor_i2c_address(sensor_id);
        auto [res, data] = _policy->i2c_read(dev_address << 1, reg, size);
        if (res != 0) {
            return RT();
        }
        auto value = static_cast<uint32_t>(*data.data());
        return RT(value);
    }

    /* Check which app (bootloader or measurement ) is running. */
    auto get_sensor_mode(TOFSensorID sensor_id) -> TOFSensorMode {
        auto ret = read_register<tmf8821::AppID>(sensor_id);
        if (!ret.has_value()) {
            return TOFSensorMode::UNKNOWN;
        }
        auto appid = static_cast<tmf8821::AppID>(ret.value()).appid;
        return static_cast<TOFSensorMode>(appid);
    }

    auto reset_custom_address() -> void { _custom_address = false; }

    // Gets the sensor i2c address
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

        // Verify page change
        if (!wait_for_state(sensor_id, _registers.cfg_result.address,
                            CMD_LOAD_CONFIG_PAGE_COMMON)) {
            return false;
        }

        // Set the new address in the I2C_SLAVE_ADDRESS (0x3B) register.
        _registers.i2c_address.slave_address = address;
        if (!set_register(_registers.i2c_address, sensor_id).has_value()) {
            return false;
        }

        // Set change address I2C_ADDR_CHANGE (0x3E) to 0
        if (!set_register(_registers.i2c_addr_change, sensor_id).has_value()) {
            return false;
        }

        // Write the config page
        auto len = prepare_cmd_frame(CMD_WRITE_CONFIG_PAGE, 0, 0);
        if (!write(sensor_id, _registers.cmd_stat.address, BUFFER, len)
                 .has_value()) {
            return false;
        }

        // Apply the new address with CMD_I2C_SLAVE_ADDRESS to CMD_STAT (0x08)
        // reg. Need to wait for registers to change.
        _policy->sleep_ms(DEFAULT_SLEEP_MS * 2);
        len = prepare_cmd_frame(CMD_I2C_SLAVE_ADDRESS, 0, 0);
        if (!write(sensor_id, _registers.cmd_stat.address, BUFFER, len)
                 .has_value()) {
            return false;
        }

        // Use the new i2c address to verify comms.
        _custom_address = true;
        return wait_for_state(sensor_id, _registers.cmd_stat.address, STAT_OK);
    }

  private:
    auto get_custom_i2c_address(TOFSensorID sensor_id) -> uint16_t {
        switch (sensor_id) {
            case TOF_X:
                return TOF_X_ADDRESS;
            case TOF_Z:
                return TOF_Z_ADDRESS;
            default:
                return TOF_DEFAULT_ADDRESS;
        }
    }

    template <tmf8821::TMF8821Register Reg>
    requires WritableRegister<Reg>
    auto get_register_value(Reg reg) -> RegisterSerializedTypeA {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
        value &= Reg::value_mask;
        return value;
    }

    template <tmf8821::TMF8821Register Reg>
    requires ReadableRegister<Reg>
    auto read_register(TOFSensorID sensor_id) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        auto ret = read(sensor_id, Reg::address, 1);
        if (!ret.has_value()) {
            return RT();
        }
        auto value = *reinterpret_cast<Reg*>(&ret.value());
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return RT(value);
    }

    template <tmf8821::TMF8821Register Reg>
    requires WritableRegister<Reg>
    auto set_register(Reg reg, TOFSensorID sensor_id) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = get_register_value(reg);
        auto ret = write(sensor_id, Reg::address, (uint8_t*)&value, 1);
        if (!ret.has_value()) {
            return RT();
        }
        return RT(reg);
    }

    /* Makes sure the sensor is ready for communication */
    auto set_sensor_ready(TOFSensorID sensor_id) -> bool {
        auto ret = read_register<tmf8821::Enable>(sensor_id);
        if (!ret.has_value()) {
            return false;
        }
        auto reg = static_cast<tmf8821::Enable>(ret.value());
        if (!reg.pon || !reg.cpu_ready) {
            _registers.enable.pon = 1;
            _registers.enable.powerup_select = reg.powerup_select;
            update_enable(_registers, sensor_id);
            // Check if device is ready for comms
            for (uint8_t i = 0; i < DEFAULT_RETRIES; i++) {
                // Need to wait after setting Enable reg.
                _policy->sleep_ms(DEFAULT_SLEEP_MS);
                ret = read_register<tmf8821::Enable>(sensor_id);
                if (ret.has_value()) {
                    reg = static_cast<tmf8821::Enable>(ret.value());
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
            if (!bl_send_image_chunk(sensor_id, (uint8_t*)tmf8820_image + i,
                                     chunk_size)) {
                return false;
            }
        }

        // The image was downloaded successfully, jump to the measurement app.
        if (!bl_send_ram_remap_reset(sensor_id)) {
            return false;
        }

        // Verify that the measurement app is running.
        return wait_for_state(sensor_id, _registers.app_id.address, MEASURE);
    }

    auto bl_create_checksum(const uint8_t* data, uint8_t len) -> uint8_t {
        uint8_t data_sum = 0;
        for (uint8_t i = 0; i < len; i++) {
            data_sum += data[i];
        }
        return data_sum ^ 0xFF;
    }

    // Formulate a bootloader frame in the BUFFER, returns the frame length.
    uint8_t bl_prepare_cmd_frame(uint8_t cmd, uint8_t* data, uint8_t len) {
        if (len > BUFFER_LEN) {
            return 0;
        }
        auto data_len = BL_HEADER_LEN + len;
        // Add the header
        BUFFER[0] = cmd;
        BUFFER[1] = len;
        // Copy data to the buffer starting from the header len.
        for (uint8_t i = BL_HEADER_LEN; i < data_len; i++) {
            BUFFER[i] = data[i - BL_HEADER_LEN];
        }
        // Add the checksum
        BUFFER[data_len] = bl_create_checksum(BUFFER, data_len);
        return data_len + BL_FOOTER_LEN;
    }

    // Send DOWNLOAD_INIT (0x14) to start the RAM download
    auto bl_send_download_init(TOFSensorID sensor_id) -> bool {
        uint8_t data[] = {tmf8820_image[0]};
        auto f_len = bl_prepare_cmd_frame(BL_DOWNLOAD_INIT, data, 1);
        if (!write(sensor_id, _registers.bl_stat.address, BUFFER, f_len)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, _registers.bl_stat.address,
                              STAT_READY);
    }

    // Send ADDR_RAM (0x43) command to set RAM pointer to given address.
    auto bl_send_set_address(TOFSensorID sensor_id, uint16_t address) -> bool {
        int data[] = {address & 0xFF, (address & 0xFF00) >> 8};  // lsb, msb
        auto f_len = bl_prepare_cmd_frame(BL_ADDR_RAM, (uint8_t*)data, 2);
        if (!write(sensor_id, _registers.bl_stat.address, BUFFER, f_len)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, _registers.bl_stat.address,
                              STAT_READY);
    }

    // Send W_RAM (0x41) command to write RAM region with the given data.
    auto bl_send_image_chunk(TOFSensorID sensor_id, uint8_t* data, uint8_t len)
        -> bool {
        auto f_len = bl_prepare_cmd_frame(BL_W_RAM, data, len);
        if (!write(sensor_id, _registers.bl_stat.address, BUFFER, f_len)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, _registers.bl_stat.address,
                              STAT_READY);
    }

    // Send RAMREMAP_RESET (0x11) command to jump to the application.
    auto bl_send_ram_remap_reset(TOFSensorID sensor_id) -> bool {
        auto f_len = bl_prepare_cmd_frame(BL_RAMREMAP_RESET, 0, 0);
        if (!write(sensor_id, _registers.bl_stat.address, BUFFER, f_len)) {
            return false;
        }

        // RAMREMAP_RESET jumps to the application if successful, BL commands
        // wont work anymore. Need to wait ~1s after resetting to let the
        // measure application start before checking CMD_STAT.
        _policy->sleep_ms(DEFAULT_SLEEP_MS);
        return wait_for_state(sensor_id, _registers.cmd_stat.address, STAT_OK);
    }

    // Formulate a CMD frame
    uint8_t prepare_cmd_frame(uint8_t cmd, uint8_t* data, uint8_t len) {
        if (len > BUFFER_LEN) {
            return 0;
        }
        // Add the header
        BUFFER[0] = cmd;
        // Copy data to the buffer starting from the header len.
        for (uint8_t i = 1; i < len; i++) {
            BUFFER[i] = data[i - 1];
        }
        return len + 1;
    }

    // Changes the i2c page for commands.
    auto change_config_page(TOFSensorID sensor_id, uint8_t page) -> bool {
        BUFFER[0] = page;
        if (!write(sensor_id, _registers.cmd_stat.address, BUFFER, 1)
                 .has_value()) {
            return false;
        }
        return wait_for_state(sensor_id, _registers.cmd_stat.address, STAT_OK);
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
            _policy->sleep_ms(sleep_ms);
        }
        return false;
    }

    auto configure_sensor(const TMF8821RegisterMap& registers,
                          TOFSensorID sensor_id) -> bool {
        if (!update_enable(registers, sensor_id)) {
            return false;
        }
        return true;
    }

    TOFSensorPolicy* _policy{nullptr};
    tmf8821::TMF8821RegisterMap _registers = {};
    bool _custom_address = false;
    uint8_t BUFFER[BUFFER_LEN] = {0};
};

}  // namespace tmf8821
