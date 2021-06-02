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
#include "hal/double_buffer.hpp"
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"

struct CommsTaskFreeRTOS {
    USBD_CDC_ItfTypeDef cdc_class_fops;
    USBD_HandleTypeDef usb_handle;
    USBD_CDC_LineCodingTypeDef linecoding;

    double_buffer::DoubleBuffer<char, CDC_DATA_HS_MAX_PACKET_SIZE * 4> rx_buf;
    double_buffer::DoubleBuffer<char, CDC_DATA_HS_MAX_PACKET_SIZE * 4> tx_buf;
    char *committed_rx_buf_ptr;
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
    .committed_rx_buf_ptr = nullptr};

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

extern "C" {
extern __ALIGN_BEGIN uint8_t
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    USBD_CDC_CfgHSDesc[USB_CDC_CONFIG_DESC_SIZ] __ALIGN_END;
extern __ALIGN_BEGIN uint8_t
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    USBD_CDC_CfgFSDesc[USB_CDC_CONFIG_DESC_SIZ] __ALIGN_END;
}

// Actual function that runs in the task
void run(void *param) {  // NOLINT(misc-unused-parameters)
    auto *task_pair = static_cast<decltype(_tasks) *>(param);
    auto *local_task = task_pair->second;
    auto *top_task = task_pair->first;
    // This clears the capability bit that would be other sent upstream
    // indicating we handle flow control line setting from host, which we don't,
    // which leads to delays and annoying kernel messages. See
    // stm32f303-bsp/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c:174
    // for annotated descriptor definitions
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    USBD_CDC_CfgHSDesc[30] = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    USBD_CDC_CfgFSDesc[30] = 0;
    USBD_Init(&local_task->usb_handle, &CDC_Desc, 0);
    USBD_RegisterClass(&local_task->usb_handle, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&local_task->usb_handle,
                               &local_task->cdc_class_fops);
    USBD_SetClassConfig(&local_task->usb_handle, 0);
    USBD_Start(&local_task->usb_handle);
    local_task->committed_rx_buf_ptr = local_task->rx_buf.committed()->data();
    while (true) {
        char *tx_end =
            top_task->run_once(local_task->tx_buf.accessible()->begin(),
                               local_task->tx_buf.accessible()->end());
        if (!top_task->may_connect()) {
            USBD_Stop(&_local_task.usb_handle);
        } else if (tx_end != local_task->tx_buf.accessible()->data()) {
            local_task->tx_buf.swap();
            USBD_CDC_SetTxBuffer(
                &_local_task.usb_handle,
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                reinterpret_cast<uint8_t *>(
                    local_task->tx_buf.committed()->data()),
                tx_end - local_task->tx_buf.committed()->data());
            USBD_CDC_TransmitPacket(&_local_task.usb_handle);
        }
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
                                                          .task = &_top_task};
}
}  // namespace host_comms_control_task

static auto CDC_Init() -> int8_t {
    using namespace host_comms_control_task;
    _local_task.committed_rx_buf_ptr = _local_task.rx_buf.committed()->data();
    USBD_CDC_SetRxBuffer(
        &_local_task.usb_handle,
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<uint8_t *>(_local_task.committed_rx_buf_ptr));
    USBD_CDC_ReceivePacket(&_local_task.usb_handle);
    return (0);
}

static auto CDC_DeInit() -> int8_t {
    /*
       Add your deinitialization code here
    */
    using namespace host_comms_control_task;
    _local_task.committed_rx_buf_ptr = _local_task.rx_buf.committed()->data();
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

/*
** CDC_Receive is a callback hook invoked from the CDC class internals in an
**
** interrupt context. Buf points to the pre-provided rx buf, into which the data
** from the hardware-isolated USB packet memory area has been copied; Len is a
** pointer to the length of data.
**
** Because the host may send any number of characters in one USB packet - for
** instance, a host that is using programmatic access to the serial device may
** send an entire message, while a host that is someone typing into a serial
** terminal may send one character per packet - we have to accumulate characters
** somewhere until a full message is assembled. To avoid excessive copying, we
** do this by changing the exact location of the rx buffer we give the
** USB infrastructure. The rules are
**
** - We always start after a buffer swap with the beginning of the committed
**   buffer
** - When we receive a message
**   - if there's a newline (indicating a complete message), we swap the buffers
**     and  send the one that just got swapped out to the task for parsing
**   - if there's not a newline,
**     - if, after the message we just received, there is not enough space for
**       an entire packet in the buffer, we swap the buffers and send the
**       swapped-out one to the task, where it will probably be ignored
**     - if, after the message we just received, there's enough space in the
**       buffer, we don't swap the buffers, but advance our read pointer to just
**       after the message we received
**
** Just about every line of this function has a NOLINT annotation. This is bad.
** But the goal is that this is one of a very few functions like this, and
** changes to this function require extra scrutiny and testing.
*/

// NOLINTNEXTLINE(readability-non-const-parameter)
static auto CDC_Receive(uint8_t *Buf, uint32_t *Len) -> int8_t {
    using namespace host_comms_control_task;
    ssize_t remaining_buffer_count =
        (_local_task.rx_buf.committed()->data()
         // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
         + _local_task.rx_buf.committed()->size()) -
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<char *>(Buf + *Len);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if ((std::find_if(Buf, Buf + *Len,
                      [](auto ch) { return ch == '\n' || ch == '\r'; }) !=
         (Buf +  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          *Len)) ||
        remaining_buffer_count <
            static_cast<ssize_t>(CDC_DATA_HS_MAX_PACKET_SIZE)) {
        // there was a newline in this message, can pass on
        auto message =
            messages::HostCommsMessage(messages::IncomingMessageFromHost{
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                .buffer = reinterpret_cast<const char *>(
                    _local_task.rx_buf.committed()->data()),
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                .limit = reinterpret_cast<const char *>(
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    Buf + *Len)});
        static_cast<void>(
            _top_task.get_message_queue().try_send_from_isr(message));
        _local_task.rx_buf.swap();
        _local_task.committed_rx_buf_ptr =
            _local_task.rx_buf.committed()->data();
    } else {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        _local_task.committed_rx_buf_ptr += *Len;
    }

    USBD_CDC_SetRxBuffer(
        &_local_task.usb_handle,
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<uint8_t *>(_local_task.committed_rx_buf_ptr));
    USBD_CDC_ReceivePacket(&_local_task.usb_handle);
    return USBD_OK;
}
