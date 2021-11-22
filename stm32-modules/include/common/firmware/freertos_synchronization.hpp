#pragma once

#include "FreeRTOS.h"
#include "semphr.h"

namespace freertos_synchronization {

class FreeRTOSMutex {
  public:
    FreeRTOSMutex() { handle = xSemaphoreCreateMutexStatic(&static_data); }
    FreeRTOSMutex(const FreeRTOSMutex &) = delete;
    FreeRTOSMutex(const FreeRTOSMutex &&) = delete;
    FreeRTOSMutex &operator=(const FreeRTOSMutex &) = delete;
    FreeRTOSMutex &&operator=(const FreeRTOSMutex &&) = delete;

    ~FreeRTOSMutex() { vSemaphoreDelete(handle); }

    void acquire() { xSemaphoreTake(handle, portMAX_DELAY); }

    void release() { xSemaphoreGive(handle); }

    int get_count() { return uxSemaphoreGetCount(handle); }

  private:
    SemaphoreHandle_t handle{};
    StaticSemaphore_t static_data{};
};

class FreeRTOSMutexFromISR {
  public:
    FreeRTOSMutexFromISR() {
        handle = xSemaphoreCreateMutexStatic(&static_data);
    }
    FreeRTOSMutexFromISR(const FreeRTOSMutexFromISR &) = delete;
    FreeRTOSMutexFromISR(const FreeRTOSMutexFromISR &&) = delete;
    FreeRTOSMutexFromISR &operator=(const FreeRTOSMutexFromISR &) = delete;
    FreeRTOSMutexFromISR &&operator=(const FreeRTOSMutexFromISR &&) = delete;

    ~FreeRTOSMutexFromISR() { vSemaphoreDelete(handle); }

    void acquire() {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreTakeFromISR(handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken != pdFALSE) {
            taskYIELD();
        }
    }

    void release() {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken != pdFALSE) {
            taskYIELD();
        }
    }

    int get_count() { return uxSemaphoreGetCount(handle); }

  private:
    SemaphoreHandle_t handle{};
    StaticSemaphore_t static_data{};
};

class FreeRTOSCriticalSection {
  public:
    FreeRTOSCriticalSection() {}
    FreeRTOSCriticalSection(const FreeRTOSCriticalSection &) = delete;
    FreeRTOSCriticalSection(const FreeRTOSCriticalSection &&) = delete;
    FreeRTOSCriticalSection &operator=(const FreeRTOSCriticalSection &) =
        delete;
    FreeRTOSCriticalSection &&operator=(const FreeRTOSCriticalSection &&) =
        delete;

    void acquire() { taskENTER_CRITICAL(); }

    void release() { taskEXIT_CRITICAL(); }
};

}  // namespace freertos_synchronization
