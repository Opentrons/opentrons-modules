#include "simulator/thermal_plate_thread.hpp"

#include <chrono>
#include <stop_token>

#include "simulator/sim_at24c0xc_policy.hpp"
#include "systemwide.h"
#include "thermocycler-gen2/errors.hpp"
#include "thermocycler-gen2/tasks.hpp"
#include "thermocycler-gen2/thermal_general.hpp"

using namespace thermal_plate_thread;

struct SimPeltier {
    SimPeltier() { reset(); }
    float power = 0.0F;
    PeltierDirection direction = PeltierDirection::PELTIER_HEATING;

    auto reset() -> void {
        power = 0.0F;
        direction = PeltierDirection::PELTIER_HEATING;
    }

    auto signed_power() -> double {
        return power * (direction == PELTIER_HEATING ? 1.0 : -1.0);
    }
};

using namespace at24c0xc_sim_policy;

struct SimThermalPlatePolicy
    : public SimAT24C0XCPolicy<SimThermalPlateTask::EEPROM_PAGES> {
  private:
    bool _enabled = false;
    SimPeltier _left = SimPeltier();
    SimPeltier _center = SimPeltier();
    SimPeltier _right = SimPeltier();
    double _fan_power = 0.0F;
    std::shared_ptr<periodic_data_thread::PeriodicDataThread> _periodic_data;

    using GetPeltierT = std::optional<std::reference_wrapper<SimPeltier>>;
    auto get_peltier_from_id(PeltierID peltier) -> GetPeltierT {
        if (peltier == PeltierID::PELTIER_LEFT) {
            return GetPeltierT(_left);
        }
        if (peltier == PeltierID::PELTIER_RIGHT) {
            return GetPeltierT(_right);
        }
        if (peltier == PeltierID::PELTIER_CENTER) {
            return GetPeltierT(_center);
        }
        return GetPeltierT();
    }

  public:
    using EepromPolicy = SimAT24C0XCPolicy<SimThermalPlateTask::EEPROM_PAGES>;

    SimThermalPlatePolicy(
        std::shared_ptr<periodic_data_thread::PeriodicDataThread> periodic_data)
        : EepromPolicy(), _periodic_data(periodic_data) {}

    auto set_enabled(bool enabled) -> void {
        _enabled = enabled;
        if (!enabled) {
            _left.reset();
            _center.reset();
            _right.reset();
        }
    }

    auto set_peltier(PeltierID peltier, double power,
                     PeltierDirection direction) -> bool {
        auto handle = get_peltier_from_id(peltier);
        if (!handle.has_value()) {
            return false;
        }
        if (power == 0.0F) {
            direction = PELTIER_HEATING;
        }
        handle.value().get().direction = direction;
        handle.value().get().power = power;

        return true;
    }
    auto get_peltier(PeltierID peltier) -> std::pair<PeltierDirection, double> {
        using RT = std::pair<PeltierDirection, double>;
        auto handle = get_peltier_from_id(peltier);
        if (!handle.has_value()) {
            return RT(PeltierDirection::PELTIER_COOLING, 0.0F);
        }
        return RT(handle.value().get().direction, handle.value().get().power);
    }

    auto set_fan(double power) -> bool {
        power = std::clamp(power, (double)0.0F, (double)1.0F);
        _fan_power = power;
        return true;
    }

    auto get_fan() -> double { return _fan_power; }

    auto get_fan_rpm() -> std::pair<double, double> {
        double val = 5000.0F * _fan_power;
        return std::make_pair(val, val);
    }

    auto send_power() -> void {
        _periodic_data->send_message(periodic_data_thread::PeriodicDataMessage(
            periodic_data_thread::PeltierPower{
                .left = _left.signed_power(),
                .center = _center.signed_power(),
                .right = _right.signed_power()}));
    }
};

struct thermal_plate_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimThermalPlateTask::Queue()),
          task(SimThermalPlateTask(queue)) {}
    SimThermalPlateTask::Queue queue;
    SimThermalPlateTask task;
};

auto run(
    std::stop_token st, std::shared_ptr<TaskControlBlock> tcb,
    std::shared_ptr<periodic_data_thread::PeriodicDataThread> periodic_data)
    -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimThermalPlatePolicy(periodic_data);
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            tcb->task.run_once(policy);
            policy.send_power();
        } catch (const SimThermalPlateTask::Queue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto thermal_plate_thread::build(
    std::shared_ptr<periodic_data_thread::PeriodicDataThread> periodic_data)
    -> tasks::Task<std::unique_ptr<std::jthread>, SimThermalPlateTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb, periodic_data),
                       &tcb->task);
}
