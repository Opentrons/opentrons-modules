/*
 * firmware-specific internals and hooks for motor control
 */
#include "firmware/freertos_motor_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "task.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
#include "stm32f3xx_hal.h"
#include "mc_tasks.h"
#include "mc_config.h"
#include "mc_interface.h"
#include "mc_tuning.h"
#pragma GCC diagnostic pop

#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/motor_task.hpp"
#include "heater-shaker/tasks.hpp"

#include "hardware_setup.h"
#include "motor_policy.hpp"

namespace motor_control_task {

struct MotorTaskFreeRTOS {
    TaskHandle_t main_task;
    TaskHandle_t control_task;
    ADC_HandleTypeDef hadc1;
    ADC_HandleTypeDef hadc2;
    TIM_HandleTypeDef htim1;
    TIM_HandleTypeDef htim2;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    MCI_Handle_t *pMCI[NBR_OF_MOTORS];
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    MCT_Handle_t *pMCT[NBR_OF_MOTORS];
};

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<motor_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _motor_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                 "Motor Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = motor_task::MotorTask(_motor_queue);

static constexpr uint32_t main_stack_size = 500;
static constexpr uint32_t mc_stack_size = 128;


std::array<StackType_t, mc_stack_size> control_task_stack;
// Stack as a std::array because why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<StackType_t, main_stack_size> stack;
// Internal FreeRTOS data structure for the task

// NOLINTNEXLITN(cppcoreguidelines-avoid-non-const-global-variables)
StaticTask_t main_data;
// NOLINTNEXLITN(cppcoreguidelines-avoid-non-const-global-variables)
StaticTask_t control_task_data;

static MotorTaskFreeRTOS _local_task;


// Actual function that runs inside the task
void run(void *param) {
  motor_hardware_setup(
      &_local_task.hadc1, &_local_task.hadc2,
      &_local_task.htim1, &_local_task.htim2,
      _local_task.pMCI, _local_task.pMCT);

  auto policy = MotorPolicy(_local_task.pMCI[0]);
  while (true) {
    _task.run_once(policy);
  }
}

void run_control_task(void *param) {
    static_cast<void>(param);
    while (true) {
        vTaskDelay(1);
        uint16_t code = MC_RunMotorControlTasks();
        if (code != 0) {
            auto &queue = _task.get_message_queue();
            static_cast<void>(queue.try_send(messages::MotorMessage(
                messages::MotorSystemErrorMessage{.errors = code})));
        }
    }
}

// Starter function that creates and spins off the task
auto start()
    -> tasks::Task<TaskHandle_t, motor_task::MotorTask<FreeRTOSMessageQueue>> {


    auto *handle = xTaskCreateStatic(run, "MotorControl", stack.size(), &_task,
                                     1, stack.data(), &main_data);
    auto *control_task_handle = xTaskCreateStatic(
        run_control_task, "MCControl", control_task_stack.size(), nullptr, 2,
        control_task_stack.data(), &control_task_data);
    _local_task.control_task = control_task_handle;
    _local_task.main_task = handle;
    _motor_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}

}  // namespace motor_control_task
