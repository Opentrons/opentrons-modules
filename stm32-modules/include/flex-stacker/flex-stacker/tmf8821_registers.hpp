#pragma once
/*
 * TMF8821
 *
 * Datasheet:
 * https://look.ams-osram.com/m/52236c476132a095/original/TMF8820-21-28-Multizone-Time-of-Flight-Sensor.pdf
 *
 * dToF (direct Time of Flight) wide field of view optical distance sensor
 * module.
 *
 */

/* TODO
 *
 * 1. We have 2 TOF sensors so this address needs to be set at runtime for each
 * one. We should use non-default i2c addresses to be sure we arent talking to
 *  a sensor that has been reset for example.
 *
 * 2. Condense into large registers like AMBIENT_LIGHT(0-4) and read and read
 * the number of bytes required instead of each individual byte.
 *
 * */
#include <sys/types.h>
#include <cstdint>

namespace tmf8821 {

// Any appid, any cid_rid – Registers always available
enum class BaseRegisters : uint8_t {
    APPID = 0x00,
    MINOR = 0x01,
    ENABLE = 0xE0,
    INT_STATUS = 0xE1,
    INT_ENAB = 0xE2,
    ID_REGISTER = 0xE3,
    REVID = 0xE4,
};

// appid=0x03, any cid_rid - Main Application Registers
enum class AppRegisters : uint8_t {
    PATCH = 0x02,
    BUILD_TYPE = 0x03,
    APPLICATION_STATUS = 0x04,
    MEASURE_STATUS = 0x05,
    ALGORITHM_STATUS = 0x06,
    CALIBRATION_STATUS = 0x07,
    CMD_STAT = 0x08,
    PREV_CMD = 0x09,
    LIVE_BEAT = 0x0A,
    MODE = 0x10,  // (TMF8828 ONLY)
    ACTIVE_RANGE = 0x19,
    SERIAL_NUMBER_0 = 0x1C,
    SERIAL_NUMBER_1 = 0x1D,
    SERIAL_NUMBER_2 = 0x1E,
    SERIAL_NUMBER_3 = 0x1F,
    CONFIG_RESULT = 0x20,
    TID = 0x21,
    SIZE_LSB = 0x22,
    SIZE_MSB = 0x23,
};

// appid=0x03, cid_rid=0x10 – Measurement Results
enum class MeasurementResultsRegisters : uint8_t {
    RESULT_NUMBER = 0x24,
    NUMBER_VALID_RESULTS = 0x25,
    AMBIENT_LIGHT_0 = 0x28,
    AMBIENT_LIGHT_1 = 0x29,
    AMBIENT_LIGHT_2 = 0x2A,
    AMBIENT_LIGHT_3 = 0x2B,
    PHOTON_COUNT_0 = 0x2C,
    PHOTON_COUNT_1 = 0x2D,
    PHOTON_COUNT_2 = 0x2E,
    PHOTON_COUNT_3 = 0x2F,
    REFERENCE_COUNT_0 = 0x30,
    REFERENCE_COUNT_1 = 0x31,
    REFERENCE_COUNT_2 = 0x32,
    REFERENCE_COUNT_3 = 0x33,
    SYS_TICK_0 = 0x34,
    SYS_TICK_1 = 0x35,
    SYS_TICK_2 = 0x36,
    SYS_TICK_3 = 0x37,
    // The condifence and distance registers are 0-35
    // starting at address 0x38 to 0xA3
    // so there are 108? registers in total?
    // There are 3 values per reading
    // 1 confidence level
    // 1 distance lsb
    // 1 distance msb
    // so could just have the start and end register address and could index
    // into the register like so. reading = register[0x38+index]
    // // TODO: verify this
    RES_CONFIDENCE_0 = 0x38,
    RES_DISTANCE_0_LSB = 0x39,
    RES_DISTANCE_0_MSB = 0x3A,
    RES_CONFIDENCE_1 = 0x3B,
    RES_DISTANCE_1_LSB = 0x3C,
    RES_DISTANCE_1_MSB = 0x3D,
    RES_CONFIDENCE_35 = 0xA1,
    RES_DISTANCE_35_LSB = 0xA2,
    RES_DISTANCE_35_MSB = 0xA3,
};

// appid=0x03, cid_rid=0x16 – Configuration Page
enum class ConfigurationRegisters : uint8_t {
    PERIOD_MS_LSB = 0x24,
    PERIOD_MS_MSB = 0x25,
    KILO_ITERATIONS_LSB = 0x26,
    KILO_ITERATIONS_MSB = 0x27,
    INT_THRESHOLD_LOW_LSB = 0x28,
    INT_THRESHOLD_LOW_MSB = 0x29,
    INT_THRESHOLD_HIGH_LSB = 0x2A,
    INT_THRESHOLD_HIGH_MSB = 0x2B,
    INT_ZONE_MASK_0 = 0x2C,
    INT_ZONE_MASK_1 = 0x2D,
    INT_ZONE_MASK_2 = 0x2E,
    INT_PERSISTENCE = 0x2F,
    CONFIDENCE_THRESHOLD = 0x30,
    GPIO_0 = 0x31,
    GPIO_1 = 0x32,
    POWER_CFG = 0x33,
    SPAD_MAP_ID = 0x34,
    ALG_SETTING_0 = 0x35,
    HIST_DUMP = 0x39,
    SPREAD_SPECTRUM = 0x3A,
    I2C_SLAVE_ADDRESS = 0x3B,
    OSC_TRIM_VALUE_LSB = 0x3C,
    OSC_TRIM_VALUE_MSB = 0x3D,
    I2C_ADDR_CHANGE = 0x3E,
};

// appid=0x03, cid_rid=0x17/0x18 – User defined SPAD Configuration
enum class UserSPADConfigRegisters : uint8_t {
    SPAD_ENABLE_FIRST = 0x24,
    SPAD_ENABLE_LAST = 0x41,
    SPAD_TDC_FIRST = 0x42,
    SPAD_TDC_LAST = 0x8C,
    SPAD_X_OFFSET_2 = 0x8D,
    SPAD_Y_OFFSET_2 = 0x8E,
    SPAD_X_SIZE = 0x8F,
    SPAD_Y_SIZE = 0x90,
};

// appid=0x03, cid_rid=0x19 – Factory Calibration
enum class FactoryCalibrationRegisters : uint16_t {
    FACTORY_CALIBRATION_FIRST =
        0x24,  // TODO: factory_calibration_first – see section 7.3
    CROSSTALK_ZONE1 = 0x6063,
    CROSSTALK_ZONE1_TMUX = 0x8083,
    CALIBRATION_STATUS_FC = 0xDC,
    FACTORY_CALIBRATION_LAST = 0xDF,
};

// appid=0x03, cid_rid=0x81 – Raw Data Histograms
enum class RawDataHistRegisters : uint8_t {
    SUBPACKET_NUMBER = 0x24,
    SUBPACKET_PAYLOAD = 0x25,
    SUBPACKET_CONFIG = 0x26,
    SUBPACKET_DATA0 = 0x27,
    SUBPACKET_DATA127 = 0xA6,
};

// appid=0x80 – Bootloader Registers
enum class BootloaderRegisters : uint8_t {
    BL_CMD_STAT = 0x08,
    BL_SIZE = 0x09,
    BL_DATA = 0x0A,  // bl_data0 … bl_data127 - size depends on bl_cmd_stat –
                     // can be from 0 to 128
    BL_CSUM = 0x0A,  // actual location depends on bl_cmd_stat – can be from
                     // 0x0A to 0x8B
};

enum class RegisterType : uint8_t {
    BASE = 0x00,
    MAIN_APP = 0x01,
    MEASURE_RESULT = 0x02,
    CONFIG = 0x03,
    USER_SPAD_CONFIG = 0x04,
    FACTORY_CALIBRATION = 0x05,
    RAW_DATA_HISTOGRAM = 0x06,
    BOOTLOADER = 0x07,
};

typedef struct Registers {
    BaseRegisters base;
    AppRegisters app;
    MeasurementResultsRegisters measurement;
    ConfigurationRegisters config;
    UserSPADConfigRegisters spad_config;
    FactoryCalibrationRegisters calibration;
    RawDataHistRegisters histogram;
    BootloaderRegisters bootloader;

} Registers;

inline auto is_valid_address(RegisterType type, const uint16_t reg) -> bool {
    switch (type) {
        case RegisterType::BASE:
            return (reg >= static_cast<uint8_t>(BaseRegisters::APPID) &&
                    reg <= static_cast<uint8_t>(BaseRegisters::REVID));
        case RegisterType::MAIN_APP:
            return (reg >= static_cast<uint8_t>(AppRegisters::PATCH) &&
                    reg <= static_cast<uint8_t>(AppRegisters::SIZE_MSB));
        case RegisterType::MEASURE_RESULT:
            return (reg >= static_cast<uint8_t>(
                               MeasurementResultsRegisters::RESULT_NUMBER) &&
                    reg <=
                        static_cast<uint8_t>(
                            MeasurementResultsRegisters::RES_DISTANCE_35_MSB));
        case RegisterType::CONFIG:
            return (reg >= static_cast<uint8_t>(
                               ConfigurationRegisters::PERIOD_MS_LSB) &&
                    reg <= static_cast<uint8_t>(
                               ConfigurationRegisters::I2C_ADDR_CHANGE));
        case RegisterType::USER_SPAD_CONFIG:
            return (reg >= static_cast<uint8_t>(
                               UserSPADConfigRegisters::SPAD_ENABLE_FIRST) &&
                    reg <= static_cast<uint8_t>(
                               UserSPADConfigRegisters::SPAD_Y_SIZE));
        case RegisterType::FACTORY_CALIBRATION:
            return (
                reg >= static_cast<uint16_t>(FactoryCalibrationRegisters::
                                                 FACTORY_CALIBRATION_FIRST) &&
                reg <=
                    static_cast<uint16_t>(
                        FactoryCalibrationRegisters::FACTORY_CALIBRATION_LAST));
        case RegisterType::RAW_DATA_HISTOGRAM:
            return (reg >= static_cast<uint8_t>(
                               RawDataHistRegisters::SUBPACKET_NUMBER) &&
                    reg <= static_cast<uint8_t>(
                               RawDataHistRegisters::SUBPACKET_DATA127));
        case RegisterType::BOOTLOADER:
            return (
                reg >= static_cast<uint8_t>(BootloaderRegisters::BL_CMD_STAT) &&
                reg <= static_cast<uint8_t>(BootloaderRegisters::BL_CSUM));
    }
    return false;
}

/** Template concept to constrain what structures encapsulate registers.*/
template <typename Reg>
// Struct has a valid register address
// Struct has an integer with the total number of bits in a register.
// This is used to mask the value before writing it to the sensor.
concept TMF8821Register =
    std::same_as<std::remove_cvref_t<decltype(Reg::mode)>,
                 std::remove_cvref_t<RegisterType&>> &&
    std::integral<decltype(Reg::address)> &&
    std::integral<decltype(Reg::value_mask)>;

template <typename Reg>
concept WritableRegister = requires() {
    {Reg::writable};
};

template <typename Reg>
concept ReadableRegister = requires() {
    {Reg::readable};
};

struct __attribute__((packed, __may_alias__)) AppID {
    static constexpr auto mode = RegisterType::BASE;
    static constexpr auto address = (uint16_t) BaseRegisters::APPID;
    static constexpr bool readable = true;
    static constexpr bool writable = false;
    static constexpr uint32_t value_mask = (1 << 8) - 1;

    uint8_t C7 : 8 = 0;
};

struct __attribute__((packed, __may_alias__)) Minor {
    static constexpr auto mode = RegisterType::BASE;
    static constexpr auto address = (uint16_t) BaseRegisters::MINOR;
    static constexpr bool readable = true;
    static constexpr bool writable = false;
    static constexpr uint32_t value_mask = (1 << 8) - 1;

    uint8_t minor : 8 = 0;
};

struct __attribute__((packed, __may_alias__)) Enable {
    static constexpr auto mode = RegisterType::BASE;
    static constexpr auto address = (uint16_t) BaseRegisters::ENABLE;
    static constexpr bool readable = true;
    static constexpr bool writable = true;
    static constexpr uint32_t value_mask = (1 << 8) - 1;
    // TODO: Do we need the size of the register?

    // Do these need to be uint32_t?
    uint8_t pon : 1 = 1;
    uint8_t padding_1 : 4 = 0;
    uint8_t powerup_select : 2 = 0;
    uint8_t cpu_ready : 1 = 0;
};

struct TMF8821RegisterMap {
    Enable enable = {};
};

// Registers are all 32 bits
using RegisterSerializedType = uint32_t;
// Type definition to allow type aliasing for pointer dereferencing
using RegisterSerializedTypeA = __attribute__((__may_alias__)) uint32_t;
}  // namespace tmf8821
