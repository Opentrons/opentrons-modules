#pragma once

#include <array>
#include <functional>
#include <tuple>

#include "FreeRTOS.h"
#include "ot_utils/core/logging.h"
#include "task.h"

namespace ot_utils {
namespace freertos_task {

/**
 * A FreeRTOS task.
 *
 * @tparam StackDepth The size of the stack.
 * @tparam EntryPoint The entry point type. Must be a callable that
 * takes one argument.
 * @tparam TaskArgs Pointer types of arguments passed to EntryPoint
 */
template <uint32_t StackDepth, typename EntryPoint>
class FreeRTOSTask {
  public:
    FreeRTOSTask(EntryPoint& entry_point) : entry_point{entry_point} {}
    auto operator=(FreeRTOSTask&) -> FreeRTOSTask& = delete;
    auto operator=(FreeRTOSTask&&) -> FreeRTOSTask&& = delete;
    FreeRTOSTask(FreeRTOSTask&) = delete;
    FreeRTOSTask(FreeRTOSTask&&) = delete;
    ~FreeRTOSTask() = default;

    /**
     * Constructor
     * @param priority The tasks priority to use
     * @param task_name Task name.
     * @param task_args Data passed into task entry point.
     */
    template <typename... TaskArgs>
    void start(UBaseType_t priority, const char* task_name,
               TaskArgs*... task_args) {
        using InstanceDataType = std::pair<void*, std::tuple<TaskArgs*...>>;

        InstanceDataType instance_data{this, std::make_tuple(task_args...)};
        starter = [instance_data]() -> void {
            auto instance = static_cast<FreeRTOSTask<StackDepth, EntryPoint>*>(
                instance_data.first);
            LOG("Entering task: %s", pcTaskGetName(instance->handle));
            // Call the entry point with the argument
            std::apply(instance->entry_point, instance_data.second);
        };
        handle = xTaskCreateStatic(f, task_name, backing.size(), this, priority,
                                   backing.data(), &static_task);
    }

  private:
    /**
     * Entry point passed to the task.
     * @param v This instance
     */
    static void f(void* v) {
        auto task_obj = static_cast<FreeRTOSTask<StackDepth, EntryPoint>*>(v);
        if (task_obj && task_obj->starter) {
            task_obj->starter();
        } else {
            LOG("Could not start task");
        }
    }

    TaskHandle_t handle{nullptr};
    StaticTask_t static_task{};
    std::array<StackType_t, StackDepth> backing{};
    EntryPoint& entry_point;

    std::function<void()> starter{};
};

}  // namespace freertos_task
}  // namespace ot_utils
