#include "simulator/heater_thread.hpp"

#include <cstdint>
#include <memory>
#include <stop_token>
#include <thread>

#include "core/thermistor_conversion.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"

struct SimHeaterPolicy {
    [[nodiscard]] auto power_good() const -> bool { return true; }
    [[nodiscard]] auto try_reset_power_good() -> bool { return true; };
    auto set_power_output(double relative_power) -> void {
        power = relative_power;
    };
    auto disable_power_output() -> void { power = 0; }

  private:
    double power = 0;
};

struct heater_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimHeaterTask::Queue()), task(SimHeaterTask(queue)) {}
    SimHeaterTask::Queue queue;
    SimHeaterTask task;
};

auto run(std::stop_token st,
         std::shared_ptr<heater_thread::TaskControlBlock> tcb) -> void {
    auto policy = SimHeaterPolicy();
    auto converter = thermistor_conversion::Conversion(
        thermistor_conversion::ThermistorType::NTCG104ED104DTDSX,
        heater_thread::SimHeaterTask::THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM,
        heater_thread::SimHeaterTask::ADC_BIT_DEPTH);
    tcb->queue.set_stop_token(st);
    auto conversion_message = messages::TemperatureConversionComplete{
        .pad_a = converter.backconvert(25.0),
        .pad_b = converter.backconvert(25.0),
        .board = converter.backconvert(30),
    };
    tcb->queue.get_backing_queue().push(
        messages::HeaterMessage(conversion_message));
    while (!st.stop_requested()) {
        auto last_setpoint = tcb->task.get_setpoint();
        try {
            tcb->task.run_once(policy);
        } catch (const heater_thread::SimHeaterTask::Queue::StopDuringMsgWait&
                     sdmw) {
            return;
        }
        auto new_setpoint = tcb->task.get_setpoint();
        if (last_setpoint != new_setpoint) {
            auto conversion_message = messages::TemperatureConversionComplete{
                .pad_a = converter.backconvert(tcb->task.get_setpoint()),
                .pad_b = converter.backconvert(tcb->task.get_setpoint()),
                .board = converter.backconvert(30)};
            tcb->queue.get_backing_queue().push(
                messages::HeaterMessage(conversion_message));
        }
    }
}

auto heater_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimHeaterTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb), &tcb->task);
}
