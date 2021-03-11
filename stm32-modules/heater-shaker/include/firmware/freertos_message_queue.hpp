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
template <typename Message, size_t queue_size = 10>
class FreeRTOSMessageQueue {
  public:
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
        if (sent) {
            xTaskNotify(receiver_handle, 1 << sent_bit, eSetBits);
        }
        return sent;
    }
    [[nodiscard]] auto try_recv(Message* message) -> bool {
        return xQueueReceive(queue, message, 0) == pdTRUE;
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
