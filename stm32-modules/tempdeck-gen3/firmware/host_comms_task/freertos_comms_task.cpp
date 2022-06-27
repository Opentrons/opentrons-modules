/*
 * firmware-specific functions, data, and hooks for host comms control
 */
#include "firmware/freertos_comms_task.hpp"

#include <array>
#include <functional>
#include <utility>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/usb_hardware.h"
#include "hal/double_buffer.hpp"
#include "task.h"
#include "tempdeck-gen3/host_comms_task.hpp"
#include "tempdeck-gen3/messages.hpp"

/** Sadly this must be manually duplicated from usbd_cdc.h */
constexpr size_t CDC_BUFFER_SIZE = 512U;

// Store any static data for USB comms
struct CommsTaskFreeRTOS {
    double_buffer::DoubleBuffer<char, CDC_BUFFER_SIZE * 4> rx_buf;
    double_buffer::DoubleBuffer<char, CDC_BUFFER_SIZE * 4> tx_buf;
    char *committed_rx_buf_ptr;
};

static auto cdc_init_handler() -> uint8_t *;
static auto cdc_deinit_handler() -> void;
// NOLINTNEXTLINE(readability-named-parameter)
static auto cdc_rx_handler(uint8_t *, uint32_t *) -> uint8_t *;

namespace host_comms_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::HostCommsQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _comms_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                 "Comms Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static CommsTaskFreeRTOS _local_task = {
    .rx_buf = {}, .tx_buf = {}, .committed_rx_buf_ptr = nullptr};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = host_comms_task::HostCommsTask(_comms_queue, nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator *aggregator) -> void {
    auto *local_task = &_local_task;
    auto *top_task = &_top_task;

    auto *handle = xTaskGetCurrentTaskHandle();

    _comms_queue.provide_handle(handle);
    top_task->provide_aggregator(aggregator);

    usb_hw_init(&cdc_rx_handler, &cdc_init_handler, &cdc_deinit_handler);
    usb_hw_start();
    local_task->committed_rx_buf_ptr = local_task->rx_buf.committed()->data();
    while (true) {
        char *tx_end =
            top_task->run_once(local_task->tx_buf.accessible()->begin(),
                               local_task->tx_buf.accessible()->end());
        if (!top_task->may_connect()) {
            usb_hw_stop();
        } else if (tx_end != local_task->tx_buf.accessible()->data()) {
            local_task->tx_buf.swap();
            usb_hw_send(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                reinterpret_cast<uint8_t *>(
                    local_task->tx_buf.committed()->data()),
                tx_end - local_task->tx_buf.committed()->data());
        }
    }
}

}  // namespace host_comms_control_task

static auto cdc_init_handler() -> uint8_t * {
    using namespace host_comms_control_task;
    _local_task.committed_rx_buf_ptr = _local_task.rx_buf.committed()->data();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<uint8_t *>(_local_task.committed_rx_buf_ptr);
}

static auto cdc_deinit_handler() -> void {
    using namespace host_comms_control_task;
    _local_task.committed_rx_buf_ptr = _local_task.rx_buf.committed()->data();
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
static auto cdc_rx_handler(uint8_t *Buf, uint32_t *Len) -> uint8_t * {
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
        remaining_buffer_count < static_cast<ssize_t>(CDC_BUFFER_SIZE)) {
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
        static_cast<void>(_comms_queue.try_send_from_isr(message));
        _local_task.rx_buf.swap();
        _local_task.committed_rx_buf_ptr =
            _local_task.rx_buf.committed()->data();
    } else {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        _local_task.committed_rx_buf_ptr += *Len;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<uint8_t *>(_local_task.committed_rx_buf_ptr);
}
