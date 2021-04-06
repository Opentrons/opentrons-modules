/*
 * firmware-specific functions, data, and hooks for host comms control
 */
#include "firmware/freertos_comms_task.hpp"

#include <array>
#include <functional>
#include <utility>

#include "FreeRTOS.h"
#include "task.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#pragma GCC diagnostic pop

#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/tasks.hpp"

constexpr size_t MAX_USB_MESSAGE_SIZE_BYTES = 128;

struct CommsTaskFreeRTOS {
    USBD_CDC_ItfTypeDef cdc_class_fops;
    USBD_HandleTypeDef usb_handle;
    USBD_CDC_LineCodingTypeDef linecoding;
    std::array<uint8_t, MAX_USB_MESSAGE_SIZE_BYTES> rx_buf;
    std::array<uint8_t, MAX_USB_MESSAGE_SIZE_BYTES> tx_buf;
};

static auto CDC_Init() -> int8_t;
static auto CDC_DeInit() -> int8_t;

// NOLINTNEXTLINE(readability-named-parameter)
static auto CDC_Control(uint8_t, uint8_t *, uint16_t) -> int8_t;
// NOLINTNEXTLINE(readability-named-parameter)
static auto CDC_Receive(uint8_t *, uint32_t *) -> int8_t;

namespace host_comms_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static FreeRTOSMessageQueue<host_comms_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _comms_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                 "Comms Message Queue");
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static CommsTaskFreeRTOS _local_task = {
    .cdc_class_fops =
        {
            .Init = CDC_Init,
            .DeInit = CDC_DeInit,
            .Control = CDC_Control,
            .Receive = CDC_Receive,
        },
    .usb_handle = {},

    .linecoding =
        {.bitrate =
             115200,  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)
         .format = 0x00,
         .paritytype = 0x00,
         .datatype = 0x08},
    .rx_buf = {},
    .tx_buf = {},
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = host_comms_task::HostCommsTask(_comms_queue);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _tasks = std::make_pair(&_top_task, &_local_task);

static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// Internal FreeRTOS data structure
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Actual function that runs in the task
void run(void *param) {  // NOLINT(misc-unused-parameters)
    static constexpr uint32_t delay_ticks = 100;
    auto *task_pair = static_cast<decltype(_tasks) *>(param);
    auto *local_task = task_pair->second;
    USBD_Init(&local_task->usb_handle, &CDC_Desc, 0);
    USBD_RegisterClass(&local_task->usb_handle, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&local_task->usb_handle,
                               &local_task->cdc_class_fops);
    USBD_SetClassConfig(&local_task->usb_handle, 0);
    USBD_Start(&local_task->usb_handle);
    while (true) {
        vTaskDelay(delay_ticks);
    }
}

// Function that creates and spins up the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   host_comms_task::HostCommsTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "HostCommsControl", stack.size(),
                                     &_tasks, 1, stack.data(), &data);
    _comms_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_top_task)>{.handle = handle,
                                                          .task = _top_task};
}
}  // namespace host_comms_control_task

static auto CDC_Init() -> int8_t {
    using namespace host_comms_control_task;
    USBD_CDC_SetTxBuffer(&_local_task.usb_handle, _local_task.tx_buf.data(),
                         _local_task.tx_buf.size());
    USBD_CDC_SetRxBuffer(&_local_task.usb_handle, _local_task.rx_buf.data());
    return (0);
}

static auto CDC_DeInit() -> int8_t {
    /*
       Add your deinitialization code here
    */
    return (0);
}

static auto CDC_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length) -> int8_t {
    using namespace host_comms_control_task;
    static_cast<void>(length);
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
            _local_task.linecoding.bitrate = (uint32_t)(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | (pbuf[3] << 24));
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            _local_task.linecoding.format = pbuf[4];
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-magic-numbers)
            _local_task.linecoding.paritytype = pbuf[5];
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-magic-numbers)
            _local_task.linecoding.datatype = pbuf[6];
            break;

        case CDC_GET_LINE_CODING:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[0] = (uint8_t)(_local_task.linecoding.bitrate);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[1] = (uint8_t)(_local_task.linecoding.bitrate >> 8);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[2] = (uint8_t)(_local_task.linecoding.bitrate >> 16);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[3] = (uint8_t)(_local_task.linecoding.bitrate >> 24);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            pbuf[4] = _local_task.linecoding.format;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-magic-numbers)
            pbuf[5] = _local_task.linecoding.paritytype;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-magic-numbers)
            pbuf[6] = _local_task.linecoding.datatype;

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

// NOLINTNEXTLINE(readability-non-const-parameter)
static auto CDC_Receive(uint8_t *Buf, uint32_t *Len) -> int8_t {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::copy(Buf, Buf + *Len,
              host_comms_control_task::_tasks.second->tx_buf.begin());
    return USBD_CDC_TransmitPacket(
        &host_comms_control_task::_tasks.second->usb_handle);
}
