/**
 * @file usb_hardware.c
 * @author Frank Sinapi (frank.sinapi@opentrons.com)
 * @brief Firmware-specific USB HAL control code
 * @details This file exists to act as a thin wrapper around STM32 HAL
 * libraries for USB control. The goal is for each function to handle
 * \e only whatever functionality needs access to the usbd_*.h headers,
 * and leave the rest of the higher level logic to the \c freertos_comms_task
 * or other C++ code.
 */

// My header
#include "firmware/usb_hardware.h"

// System headers
#include <stdbool.h>
#include <stdint.h>

// HAL headers
#include "FreeRTOS.h"
#include "task.h"
#include "usbd_def.h"
#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_desc.h"

/** Local typedef */

struct UsbHardwareConfig {
    USBD_CDC_ItfTypeDef cdc_class_fops;
    USBD_HandleTypeDef usb_handle;
    USBD_CDC_LineCodingTypeDef linecoding;

    usb_rx_callback_t rx_callback;
    usb_cdc_init_callback_t cdc_init_callback;
    usb_cdc_deinit_callback_t cdc_deinit_callback;

    bool initialized;
};

/** Static function declaration*/

static int8_t CDC_Init();
static int8_t CDC_DeInit();
static int8_t CDC_Control(uint8_t, uint8_t *, uint16_t);
static int8_t CDC_Receive(uint8_t *, uint32_t *);

/** Local variables */

struct UsbHardwareConfig _local_config = {
    .cdc_class_fops =
        {
            .Init = CDC_Init,
            .DeInit = CDC_DeInit,
            .Control = CDC_Control,
            .Receive = CDC_Receive,
        },
    .usb_handle = {},

    .linecoding =
        {.bitrate = 115200,
         .format = 0x00,
         .paritytype = 0x00,
         .datatype = 0x08},
    
    .rx_callback = NULL,
    .cdc_init_callback = NULL,
    .cdc_deinit_callback = NULL,
    
    .initialized = false
};

/** Static function instantiation.*/
static int8_t CDC_Init() {
    if(_local_config.cdc_init_callback != NULL) {
        uint8_t *new_buf = _local_config.cdc_init_callback();
        USBD_CDC_SetRxBuffer(
            &_local_config.usb_handle, new_buf);
        USBD_CDC_ReceivePacket(&_local_config.usb_handle);
    }
    return 0;
}

static int8_t CDC_DeInit() {
    if(_local_config.cdc_deinit_callback != NULL) {
        _local_config.cdc_deinit_callback();
    }
    return 0;
}

static int8_t CDC_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length) {
    (void)length;
    switch (cmd) {
        case CDC_SEND_ENCAPSULATED_COMMAND:  // NOLINT(bugprone-branch-clone)
            break;

        case CDC_GET_ENCAPSULATED_RESPONSE:
            break;

        case CDC_SET_COMM_FEATURE:
            break;

        case CDC_GET_COMM_FEATURE:
            break;

        case CDC_CLEAR_COMM_FEATURE:
            break;

        case CDC_SET_LINE_CODING:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            _local_config.linecoding.bitrate = (uint32_t)(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | (pbuf[3] << 24));
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            _local_config.linecoding.format = pbuf[4];
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-magic-numbers)
            _local_config.linecoding.paritytype = pbuf[5];
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-magic-numbers)
            _local_config.linecoding.datatype = pbuf[6];
            break;

        case CDC_GET_LINE_CODING:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[0] = (uint8_t)(_local_config.linecoding.bitrate);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[1] = (uint8_t)(_local_config.linecoding.bitrate >> 8);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[2] = (uint8_t)(_local_config.linecoding.bitrate >> 16);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[3] = (uint8_t)(_local_config.linecoding.bitrate >> 24);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[4] = _local_config.linecoding.format;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-magic-numbers)
            pbuf[5] = _local_config.linecoding.paritytype;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-magic-numbers)
            pbuf[6] = _local_config.linecoding.datatype;

            break;

        case CDC_SET_CONTROL_LINE_STATE:  // NOLINT(bugprone-branch-clone)
            break;

        case CDC_SEND_BREAK:
            break;

        default:
            break;
    }

    return (0);
}

static int8_t CDC_Receive(uint8_t *Buf, uint32_t *Len) {
    if(_local_config.rx_callback != NULL) {
        uint8_t *new_buf = _local_config.rx_callback(Buf, Len); // C++ handles most logic here
        USBD_CDC_SetRxBuffer(
            &_local_config.usb_handle,
            new_buf);
        USBD_CDC_ReceivePacket(&_local_config.usb_handle);        
    }

    return USBD_OK;
}

/** Public function instantiation.*/

void usb_hw_init(usb_rx_callback_t rx_cb,
                 usb_cdc_init_callback_t cdc_init_cb,
                 usb_cdc_deinit_callback_t cdc_deinit_cb) {
    _local_config.rx_callback = rx_cb;
    configASSERT(_local_config.rx_callback != NULL);
    _local_config.cdc_init_callback = cdc_init_cb;
    configASSERT(_local_config.cdc_init_callback != NULL);
    _local_config.cdc_deinit_callback = cdc_deinit_cb;
    configASSERT(_local_config.cdc_deinit_callback != NULL);

    // This clears the capability bit that would be other sent upstream
    // indicating we handle flow control line setting from host, which we don't,
    // which leads to delays and annoying kernel messages. See
    // stm32g4xx-bsp/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c:159
    // for annotated descriptor definitions
    uint16_t len = 0;
    uint8_t *usb_hs_desc = USBD_CDC.GetHSConfigDescriptor(&len);
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    if ((usb_hs_desc != NULL) && (len > 30)) {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        usb_hs_desc[30] = 0;
    }
    uint8_t *usb_fs_desc = USBD_CDC.GetFSConfigDescriptor(&len);
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    if ((usb_fs_desc != NULL) && (len > 30)) {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        usb_fs_desc[30] = 0;
    }

    USBD_Init(&_local_config.usb_handle, &CDC_Desc, 0);
    USBD_RegisterClass(&_local_config.usb_handle, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&_local_config.usb_handle,
                               &_local_config.cdc_class_fops);
    USBD_SetClassConfig(&_local_config.usb_handle, 0);

    _local_config.initialized = true;
}

void usb_hw_start() {
    configASSERT(_local_config.initialized);
    USBD_Start(&_local_config.usb_handle);
}

void usb_hw_stop() {
    configASSERT(_local_config.initialized);
    USBD_Stop(&_local_config.usb_handle);
}

void usb_hw_send(uint8_t *buf, uint16_t len) {
    USBD_CDC_SetTxBuffer(
        &_local_config.usb_handle,
        buf, len);
    USBD_CDC_TransmitPacket(&_local_config.usb_handle);
}
