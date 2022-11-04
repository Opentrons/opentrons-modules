/**
 * @file periodic_data_thread.hpp
 * @brief Interface for the Periodic Data task, which generates any periodic
 * simulated message data for the Thermocycler simulator.
 */
#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <variant>

#include "simulator/simulator_queue.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace periodic_data_thread {

using Power = double;        // Percentage from -1 to +1
using Temperature = double;  // Celsius

struct HeatPadPower {
    Power power;
};

struct PeltierPower {
    Power left, center, right;
};

struct StartMotorMovement {};

using PeriodicDataMessage = std::variant<std::monostate, HeatPadPower,
                                         PeltierPower, StartMotorMovement>;

using PeriodicDataQueue = SimulatorMessageQueue<PeriodicDataMessage>;

class PeriodicDataThread {
  public:
    PeriodicDataThread(bool realtime = true);

    // Send a message to this PeriodicDataThread
    auto send_message(PeriodicDataMessage msg) -> bool;

    // Provides the task info to send data properly
    auto provide_tasks(tasks::Tasks<SimulatorMessageQueue>* other_tasks)
        -> void;

    // Should be initiated in its own jthread
    auto run(std::stop_token& st) -> void;

    // Thread safe method to signal that lid thread processed data
    auto signal_lid_thread_ready() -> void;
    // Thread safe method to signal that lid thread processed data
    auto signal_plate_thread_ready() -> void;

  private:
    // The further from room temperature an element is, the stronger
    // the draw back to room temp will be.
    auto ambient_temp_effect(Temperature temp, std::chrono::milliseconds delta)
        -> double;
    // Scale a gain constant based on the current time delta since the last
    // reading
    auto scaled_gain_effect(double gain, double power,
                            std::chrono::milliseconds delta) -> double;
    auto update_heat_pad() -> void;
    auto update_peltiers() -> void;
    auto run_motor() -> void;

    Power _heat_pad_power;
    PeltierPower _peltiers_power;
    Temperature _lid_temp, _left_temp, _center_temp, _right_temp;
    uint32_t _tick_peltiers;  // Last time a peltier message was sent
    uint32_t _tick_heater;    // Last time a heater message was sent
    uint32_t _current_tick;
    PeriodicDataQueue _queue;
    tasks::Tasks<SimulatorMessageQueue>* _task_registry;
    bool _realtime;
    // This really wants to be an std::latch, but that isn't available
    // in gcc10. Bummer :(
    std::atomic_bool _init_latch;
    // If one of these flags is set, wait until the respective thread signals
    // that it read the temperature update
    std::atomic_bool _waiting_for_lid_thread{false};
    std::atomic_bool _waiting_for_plate_thread{false};
};

auto build(bool realtime) -> std::pair<std::unique_ptr<std::jthread>,
                                       std::shared_ptr<PeriodicDataThread>>;

};  // namespace periodic_data_thread
