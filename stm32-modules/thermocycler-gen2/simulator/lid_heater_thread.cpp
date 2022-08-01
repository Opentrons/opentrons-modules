#include "simulator/lid_heater_thread.hpp"

#include <chrono>
#include <stop_token>

#include "systemwide.h"
#include "thermocycler-gen2/errors.hpp"
#include "thermocycler-gen2/tasks.hpp"

using namespace lid_heater_thread;

class SimLidHeaterPolicy {
  private:
    double _power = 0.0F;
    bool _lid_fans = false;
    std::shared_ptr<periodic_data_thread::PeriodicDataThread> _periodic_data;

  public:
    SimLidHeaterPolicy(
        std::shared_ptr<periodic_data_thread::PeriodicDataThread> periodic_data)
        : _periodic_data(periodic_data) {}

    auto set_heater_power(double power) -> bool {
        _power = std::clamp(power, (double)0.0F, (double)1.0F);
        _periodic_data->send_message(periodic_data_thread::PeriodicDataMessage(
            periodic_data_thread::HeatPadPower{.power = _power}));
        return true;
    }

    auto get_heater_power() const -> double { return _power; }

    auto set_lid_fans(bool enable) -> void { _lid_fans = enable; }
};

struct lid_heater_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimLidHeaterTask::Queue()), task(SimLidHeaterTask(queue)) {}
    SimLidHeaterTask::Queue queue;
    SimLidHeaterTask task;
};

auto run(
    std::stop_token st, std::shared_ptr<TaskControlBlock> tcb,
    std::shared_ptr<periodic_data_thread::PeriodicDataThread> periodic_data)
    -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimLidHeaterPolicy(periodic_data);
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            tcb->task.run_once(policy);
        } catch (const SimLidHeaterTask::Queue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto lid_heater_thread::build(
    std::shared_ptr<periodic_data_thread::PeriodicDataThread> periodic_data)
    -> tasks::Task<std::unique_ptr<std::jthread>, SimLidHeaterTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb, periodic_data),
                       &tcb->task);
}
