/**
 * @file periodic_data_thread.cpp
 * @brief Generates any periodic data (temperatures, motor ticks) during
 * simulator operation.
 * @details
 * This module simulates any periodic data on the Thermocycler system.
 * Specifically, it generates periodic thermistor data for all of the
 * thermal elements and calls the Motor Step tick.
 *
 */

#include "simulator/periodic_data_thread.hpp"

#include <chrono>
#include <stop_token>

#include "simulator/lid_heater_thread.hpp"
#include "simulator/thermal_plate_thread.hpp"
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/tasks.hpp"

using namespace periodic_data_thread;

/** Default starting temperature for all thermistors.*/
static constexpr const double AMBIENT_TEMPERATURE = 23.0F;
/** Gain term for peltier outputs, from experimental data.*/
static constexpr const double PELTIER_GAIN = 3.2F;
/** Gain term for lid heater output, from experimental data.*/
static constexpr const double HEAT_PAD_GAIN = 0.72;
/**
 * Gain term for bringing temperature back down to ambient. Scaled against
 * the difference between a temperature and its ambient condition. The
 * constant is derived from rough modeling against the lid heater cooling
 * from 100ºC to ambient temperature.
 */
static constexpr const double AMBIENT_TEMPERATURE_GAIN = 0.0015;

static constexpr const auto PELTIER_PERIOD =
    thermal_plate_thread::SimThermalPlateTask::CONTROL_PERIOD_TICKS;

static constexpr const auto LID_PERIOD =
    lid_heater_thread::SimLidHeaterTask::CONTROL_PERIOD_TICKS;

PeriodicDataThread::PeriodicDataThread(bool realtime)
    : _heat_pad_power(0),
      _peltiers_power{.left = 0, .center = 0, .right = 0},
      _lid_temp(AMBIENT_TEMPERATURE),
      _left_temp(AMBIENT_TEMPERATURE),
      _center_temp(AMBIENT_TEMPERATURE),
      _right_temp(AMBIENT_TEMPERATURE),
      _tick_peltiers(0),
      _tick_heater(0),
      _current_tick(0),
      _queue(),
      _task_registry(nullptr),
      _realtime(realtime),
      _init_latch(false) {}

auto PeriodicDataThread::send_message(PeriodicDataMessage msg) -> bool {
    return _queue.try_send(msg);
}

auto PeriodicDataThread::provide_tasks(
    tasks::Tasks<SimulatorMessageQueue>* other_tasks) -> void {
    _task_registry = other_tasks;
    _init_latch.store(true);
}

auto PeriodicDataThread::run(std::stop_token& st) -> void {
    PeriodicDataMessage msg;

    while (!_init_latch.load()) {
        std::this_thread::yield();
    }

    auto actual_time = std::chrono::high_resolution_clock::now();

    while (!st.stop_requested()) {
        // -------------------------------------------------------------------
        // Update the current time, either based on real time or simulated
        // time progression.

        if (_realtime) {
            // Use the high resolution clock to get an idea of the time
            // difference
            auto actual_time_new = std::chrono::high_resolution_clock::now();
            auto tick_diff =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    actual_time_new - actual_time);
            _current_tick += tick_diff.count();
            actual_time = actual_time_new;
        } else {
            // For simulated time, increment tick by the smallest
            // increment that should matter
            _current_tick += std::min(PELTIER_PERIOD, LID_PERIOD);
        }

        // -------------------------------------------------------------------
        // Check for any updated control values

        while (_queue.try_recv(&msg)) {
            if (std::holds_alternative<HeatPadPower>(msg)) {
                // Update heat pad powers
                _heat_pad_power = std::get<HeatPadPower>(msg).power;
            } else if (std::holds_alternative<PeltierPower>(msg)) {
                // Update peltier temperatures
                _peltiers_power = std::get<PeltierPower>(msg);
            } else if (std::holds_alternative<StartMotorMovement>(msg)) {
                // TODO
            }
        }

        // -------------------------------------------------------------------
        // Update the heat pad & peltiers.

        if (((_current_tick - _tick_heater) > LID_PERIOD)) {
            // Must set flag BEFORE sending to ensure it is cleared
            // correctly.
            _waiting_for_lid_thread = true;
            if (!update_heat_pad()) {
                _waiting_for_lid_thread = false;
            }
        }
        if (((_current_tick - _tick_peltiers) > PELTIER_PERIOD)) {
            // Must set flag BEFORE sending to ensure it is cleared
            // correctly.
            _waiting_for_plate_thread = true;
            if (!update_peltiers()) {
                _waiting_for_plate_thread = false;
            }
        }

        // -------------------------------------------------------------------
        // Yield at the end of each loop to let other processes run
        if (_realtime) {
            // Sleep for 1 millisecond
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            // Yield to ensure other processes handle their messages
            while (_waiting_for_lid_thread.load() ||
                   _waiting_for_plate_thread.load()) {
                std::this_thread::yield();
            }
        }
    }
}

auto PeriodicDataThread::signal_lid_thread_ready() -> void {
    _waiting_for_lid_thread = false;
}

auto PeriodicDataThread::signal_plate_thread_ready() -> void {
    _waiting_for_plate_thread = false;
}

auto PeriodicDataThread::ambient_temp_effect(Temperature temp,
                                             std::chrono::milliseconds delta)
    -> Temperature {
    using Seconds = std::chrono::duration<double, std::chrono::seconds::period>;
    auto seconds = std::chrono::duration_cast<Seconds>(delta);
    return (AMBIENT_TEMPERATURE - temp) * AMBIENT_TEMPERATURE_GAIN *
           seconds.count();
}

auto PeriodicDataThread::scaled_gain_effect(double gain, double power,
                                            std::chrono::milliseconds delta)
    -> Temperature {
    using Seconds = std::chrono::duration<double, std::chrono::seconds::period>;
    auto seconds = std::chrono::duration_cast<Seconds>(delta);

    return seconds.count() * gain * power;
}

auto PeriodicDataThread::update_heat_pad() -> bool {
    auto converter = thermistor_conversion::Conversion<lookups::KS103J2G>(
        lid_heater_thread::SimLidHeaterTask::
            THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM,
        lid_heater_thread::SimLidHeaterTask::ADC_BIT_MAX, false);

    auto timedelta = std::chrono::milliseconds(_current_tick - _tick_heater);

    _lid_temp += scaled_gain_effect(HEAT_PAD_GAIN, _heat_pad_power, timedelta) +
                 ambient_temp_effect(_lid_temp, timedelta);
    auto message = messages::LidTempReadComplete{
        .lid_temp = converter.backconvert(_lid_temp),
        .timestamp_ms = _current_tick};
    _tick_heater = _current_tick;

    if (_task_registry) {
        if (_task_registry->lid_heater->get_message_queue().try_send(message)) {
            return true;
        }
    }
    return false;
}

auto PeriodicDataThread::update_peltiers() -> bool {
    auto converter = thermistor_conversion::Conversion<lookups::KS103J2G>(
        thermal_plate_thread::SimThermalPlateTask::
            THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM,
        thermal_plate_thread::SimThermalPlateTask::ADC_BIT_MAX, false);

    auto timedelta = std::chrono::milliseconds(_current_tick - _tick_peltiers);

    _left_temp +=
        scaled_gain_effect(PELTIER_GAIN, _peltiers_power.left, timedelta) +
        ambient_temp_effect(_left_temp, timedelta);
    _center_temp +=
        scaled_gain_effect(PELTIER_GAIN, _peltiers_power.center, timedelta) +
        ambient_temp_effect(_center_temp, timedelta);
    _right_temp +=
        scaled_gain_effect(PELTIER_GAIN, _peltiers_power.right, timedelta) +
        ambient_temp_effect(_right_temp, timedelta);

    auto message = messages::ThermalPlateTempReadComplete{
        .heat_sink = converter.backconvert(AMBIENT_TEMPERATURE),
        .front_right = converter.backconvert(_right_temp),
        .front_center = converter.backconvert(_center_temp),
        .front_left = converter.backconvert(_left_temp),
        .back_right = converter.backconvert(_right_temp),
        .back_center = converter.backconvert(_center_temp),
        .back_left = converter.backconvert(_left_temp),
        .timestamp_ms = _current_tick};

    _tick_peltiers = _current_tick;

    if (_task_registry) {
        if (_task_registry->thermal_plate->get_message_queue().try_send(
                message)) {
            return true;
        }
    }
    return false;
}

auto PeriodicDataThread::run_motor() -> void {
    // Todo!!!
}

auto periodic_data_thread::build(bool realtime)
    -> std::pair<std::unique_ptr<std::jthread>,
                 std::shared_ptr<PeriodicDataThread>> {
    auto thread = std::make_shared<PeriodicDataThread>(realtime);

    auto lambda = [](std::stop_token st,
                     std::shared_ptr<PeriodicDataThread> _thread) {
        _thread->run(st);
    };

    return std::make_pair(std::make_unique<std::jthread>(lambda, thread),
                          thread);
}
