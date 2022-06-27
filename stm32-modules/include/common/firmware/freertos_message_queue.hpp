/*
 * implementation of the MessageQueue concept that uses freertos queues
 * underneath
 */

#pragma once
#include <array>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

// It's ok to use magic numbers as default arguments
// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
template <typename M, size_t Idx = 0, size_t queue_size = 10>
class FreeRTOSMessageQueue {
  public:
    using Message = M;

    // https://bugs.llvm.org/show_bug.cgi?id=37902
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    explicit FreeRTOSMessageQueue(uint8_t notification_bit, const char* name)
        : FreeRTOSMessageQueue(notification_bit) {
        vQueueAddToRegistry(queue, name);
    }
    explicit FreeRTOSMessageQueue(uint8_t notification_bit)
        : queue_control_structure(),
          backing(),
          queue(xQueueCreateStatic(queue_size, sizeof(Message), backing.data(),
                                   &queue_control_structure)),
          receiver_handle(nullptr),
          sent_bit(notification_bit) {}

    // For use with queue_aggregator
    struct Tag {};

    // Since the FreeRTOS queue control structures intern data and we don't want
    // to mess with their internals, you cannot copy or move this. It should be
    // declared once, and passed around by reference thereafter.
    FreeRTOSMessageQueue(const FreeRTOSMessageQueue& other) = delete;
    auto operator=(const FreeRTOSMessageQueue& other)
        -> FreeRTOSMessageQueue& = delete;
    FreeRTOSMessageQueue(const FreeRTOSMessageQueue&& other) noexcept = delete;
    auto operator=(FreeRTOSMessageQueue&& other) noexcept
        -> FreeRTOSMessageQueue = delete;
    ~FreeRTOSMessageQueue() = default;
    [[nodiscard]] auto try_send(const Message& message,
                                const uint32_t timeout_ticks = 0) -> bool {
        auto sent = xQueueSendToBack(queue, &message, timeout_ticks) == pdTRUE;
        return sent;
    }

    [[nodiscard]] auto try_send_from_isr(const Message& message) -> bool {
        BaseType_t higher_woken = pdFALSE;
        auto sent = xQueueSendFromISR(queue, &message, &higher_woken);
        portYIELD_FROM_ISR(  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
            higher_woken);
        return sent;
    }
    [[nodiscard]] auto try_recv(Message* message, uint32_t timeout_ticks = 0)
        -> bool {
        return xQueueReceive(queue, message, timeout_ticks) == pdTRUE;
    }
    auto recv(Message* message) -> void {
        BaseType_t got_message = pdFALSE;
        while (got_message == pdFALSE) {
            got_message = xQueueReceive(queue, message, portMAX_DELAY);
        }
    }
    [[nodiscard]] auto has_message() const -> bool {
        return uxQueueMessagesWaiting(queue) != 0;
    }
    void provide_handle(TaskHandle_t handle) { receiver_handle = handle; }

  private:
    StaticQueue_t queue_control_structure;
    std::array<uint8_t, queue_size * sizeof(Message)> backing;
    QueueHandle_t queue;
    TaskHandle_t receiver_handle;
    uint8_t sent_bit;
};
