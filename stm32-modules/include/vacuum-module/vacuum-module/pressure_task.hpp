/**
 * The Pressure Task is responsible for regulating pressure, it is the main
 * control loop which at a high-level, reads the pressure sensors, and sends
 * messages to the PumpTask to tell the motor what rpm is needed to maintain the
 * target pressure.
 *
 * 1. The entrypoint message is the `SetPressureStateMessage` which tells this
 * task the desired target pressure to maintain. In this message we start the
 * pressure control loop by calling the `policy.pressure_control_state` function
 * which starts a second FreeRTOS task that emits a `PressureControlMessage`
 * every `CONTROL_PERIOD_MS`.
 * 2. In the `PressureControlMessage`, we read the absolute pressure sensors
 *  from the motor (A) and manifold (B).
 * 3. We pass the abs readings through an EMA filter to smooth out the values
 * 4. The Slew Rate Limiter creates a smooth ramp to the target_pressure.
 * 5. We then calculate the error by subtracting the smooth target pressure
 *  from the current pressure which is fed to a PID that outputs base rpm.
 * 6. For the total rpm we use this base rpm and add on feed-forward velocity +
 * holding.
 * 7. A `SetPumpState` message is sent to the PumpTask with the new target RPM
 *  and the cycle is repeated on the next `PressureControlMessage` tick.
 *
 * Feed-Forward (FF) Math:
 * - Velocity FF: rpm = (mbar/sec_rate) * K_VELOCITY
 * Compensates for the dynamic load of changing pressure; provides the 'push'
 * to follow the ramp.
 * - Holding FF: rpm = [(P_atm - P_target) / P_atm] * K_HOLDING
 * Compensates for static atmospheric load. As vacuum deepens (ratio 0.0
 * -> 1.0), base RPM increases to counteract leaks and back-pressure. Safety: FF
 * is disabled during overshoot (error < -2.0mbar) or target relaxation to
 * prevent the pump from fighting the natural pressure rise.
 *
 * Stopping Pressure Control:
 * The pressure control is stopped when the `_control_state.enable_pump`
 * is false. We stop the `PressureControlMessage` emitter task, send a stop
 * `SetPumpMessage` message to the PumpTask, then reset all relevant internal
 *  _control_state variables.
 */

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <variant>

#include "core/ack_cache.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/pressure_policy.hpp"
#include "hal/message_queue.hpp"
#include "lps22hb.hpp"
#include "messages.hpp"
#include "mprll0025pa00001a.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"
#include "pressure_controller.hpp"
#include "systemwide.h"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"
#include "waste_detector.hpp"

namespace pressure_task {
using lps22hb::LPS22HB;
using vacuum_pressure_sensor::MPRLL0025PA00001;
using namespace ot_utils::freertos_timer;
using pressure_controller::PressureController;
using waste_detector::WasteDetector;

constexpr uint8_t ABS_PRESSURE_A_ADDR = 0x18;  // Closest to Manifold
constexpr uint8_t ABS_PRESSURE_B_ADDR = 0x18;  // Closest to Pump
constexpr uint8_t ATM_PRESSURE_ADDR = 0x5D;

// The frequency the pressure control loop runs at.
static constexpr const uint32_t CONTROL_PERIOD_HZ = 25;
static constexpr const uint32_t CONTROL_PERIOD_MS =
    (1.0F / CONTROL_PERIOD_HZ) * 1000;
static constexpr const double MS_TO_SECONDS = 0.001F;
static constexpr const double ATM_PRESSURE_MBAR = 1013.25;
// Pump RPM slew limits aligned with pressure ramp * K_VELOCITY feed-forward.
static constexpr const double PUMP_RAMP_MIN = 1.0F;
static constexpr const double PUMP_RAMP_MAX = 500.0F;

// -- Duration Threshold --
static constexpr const uint32_t UPDATE_PERIOD_MS = 10;
static constexpr const uint32_t PRESSURE_NOT_REACHED_TIMEOUT = 2000;
// 50 samples at 25 Hz ~= 2 seconds of in-tolerance readings.
static constexpr const uint8_t PRESSURE_STATE_BUFFER_LEN = 50;
static constexpr const double MIN_PRESSURE_TOLERANCE_MBAR = 5;
// The percent the pressure could be off by and still be "target reached"
static constexpr const double REL_PRESSURE_TOLERANCE_PCT = 2.0;

// Debug buffer
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline std::array<char, DEBUG_MAX_MESSAGE_LENGTH> DEBUG_BUF = {0};

using MPRDriverType = MPRLL0025PA00001<i2c::hardware::I2C>;
using LPSDriverType = LPS22HB<i2c::hardware::I2C>;
using Driver = std::variant<MPRDriverType, LPSDriverType>;

using PressurePolicy = pressure_policy::PressurePolicy;
using Message = messages::PressureMessage;
using Error = errors::ErrorCode;

struct PressureSensor {
    PressureSensorID kind;
    Driver driver;
    PressureSensorState state = DISABLED;
    bool ok;
};

const PressureSensor abs_pressure_a = {
    .kind = ABS_PRESSURE_A,
    .driver = MPRLL0025PA00001<i2c::hardware::I2C>(ABS_PRESSURE_A_ADDR),
};

const PressureSensor abs_pressure_b = {
    .kind = ABS_PRESSURE_B,
    .driver = MPRLL0025PA00001<i2c::hardware::I2C>(ABS_PRESSURE_B_ADDR),
};

const PressureSensor atm_pressure = {
    .kind = ATM_PRESSURE,
    .driver = LPS22HB<i2c::hardware::I2C>(ATM_PRESSURE_ADDR),
};

struct PressureControlState {
    double target_pressure = 0;   // Target Guage Pressure
    double current_pressure = 0;  // Current Guage Pressure
    double target_rpm = 0;

    double pressure_abs_a = 0;
    double pressure_abs_b = 0;
    double pressure_atm = 0;

    uint32_t duration_s = 0;
    Error error = Error::NO_ERROR;
    bool target_pressure_reached = false;

    // Percentage of the intended vacuum depth
    double rel_tol_pct = REL_PRESSURE_TOLERANCE_PCT;
    uint32_t start_time_ms = 0;
    uint32_t timeout_s = 0;

    uint32_t last_tick = 0;
    bool enable_vacuum = false;
    VentState vent_state = VentState::CLOSED;
    bool vent_after = true;
    // if false, pump is driven externally; monitor only, no SetPump msgs sent
    bool control_pump = true;
};

const PressureControlState pressure_control_state;

template <typename P>
concept PressureControlPolicy = requires(P p) {
    {p.sleep_ms(1)};
    { p.get_time_ms() } -> std::same_as<uint32_t>;
    {
        p.get_i2c_comms(PressureSensorID{})
        } -> std::same_as<i2c::hardware::I2C*>;
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class PressureTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit PressureTask(Queue& q, Aggregator* aggregator,
                          PressurePolicy* policy)
        : _message_queue(q),
          _task_registry(aggregator),
          _policy(policy),
          _vacuum_timer(
              "Vacuum Timer",
              [ThisPtr = this] { ThisPtr->vacuum_timer_end_callback(); },
              UPDATE_PERIOD_MS),
          _controller(CONTROL_PERIOD_MS) {}
    PressureTask(const PressureTask& other) = delete;
    auto operator=(const PressureTask& other) -> PressureTask& = delete;
    PressureTask(PressureTask&& other) noexcept = delete;
    auto operator=(PressureTask&& other) noexcept -> PressureTask& = delete;
    ~PressureTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <PressureControlPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }

        if (!_initialized) {
            _policy = &policy;
            // Initialize pressure sensors
            for (auto sensor_id :
                 {ABS_PRESSURE_A, ABS_PRESSURE_B, ATM_PRESSURE}) {
                auto& sensor = get_sensor(sensor_id);
                sensor.state = INITIALIZING;
                auto comms = policy.get_i2c_comms(sensor_id);
                sensor.ok = std::visit(
                    [&](auto&& driver) -> bool {
                        auto ok = driver.initialize(comms, sensor_id);
                        if (ok) {
                            driver.read_pressure();
                        }
                        return ok;
                    },
                    sensor.driver);
                sensor.state = sensor.ok ? IDLE : SENSOR_ERROR;
            }

            // Slew rate is mbar/sec
            _controller.configure_slew(_control_state.pressure_abs_b,
                                       pressure_controller::DEFAULT_RAMP_RATE);

            // Close the vent
            set_vent_state(VentState::CLOSED);

            _message_queue.set_ready();
            _initialized = true;
        }

        auto message = Message(std::monostate());
        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
    template <PressureControlPolicy Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::PressureControlMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(m);
        // Get delta time
        auto timestamp = policy.get_time_ms();
        auto dt = (timestamp - _control_state.last_tick) * MS_TO_SECONDS;
        _control_state.last_tick = timestamp;
        if (_control_state.start_time_ms == 0) {
            _control_state.start_time_ms = timestamp;
        }

        // Stop vacuum control
        if (!_control_state.enable_vacuum) {
            handle_control_state_outcomes();
            return;
        }

        // Update absolute pressure
        for (auto sensor_id : {ABS_PRESSURE_A, ABS_PRESSURE_B}) {
            auto ret = update_pressure(sensor_id);
            // Reset the sensor if there is some problem
            if (ret != NO_ERROR) {
                policy.sensor_reset(sensor_id);
                continue;
            }
        }

        auto target_pressure = _control_state.target_pressure;
        auto pressure_atm = _control_state.pressure_atm;
        auto current_pressure_a = _control_state.pressure_abs_a;
        auto current_pressure_b = _control_state.pressure_abs_b;
        _control_state.current_pressure = current_pressure_b;

        // Handle duration and vent after
        pressure_state_buffer.at(pressure_state_buffer_index) =
            current_pressure_b;
        pressure_state_buffer_index =
            (pressure_state_buffer_index + 1) % PRESSURE_STATE_BUFFER_LEN;
        monitor_target_pressure();

        // Handle waste detection
        auto res =
            // NOLINTNEXTLINE(readability-suspicious-call-argument)
            _detector.check(timestamp, current_pressure_a, current_pressure_b,
                            target_pressure, pressure_atm);
        if (res != waste_detector::WasteFullError::NO_ERROR) {
            stop_vacuum();
            set_vent_state(VentState::OPENED);
            send_error_message(Error::WASTE_FULL_ERROR);
            return;
        }

        if (_control_state.control_pump) {
            auto rpm =
                _controller.update(dt, current_pressure_b, target_pressure);
            _control_state.target_rpm = rpm;
            set_pump_state(true, rpm);
        }
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetPressureStateMessage& m,
                       Policy& policy) -> void {
        // Convert target guage presure to abs pressure
        update_pressures(policy, true);
        auto p_atm = _control_state.pressure_atm;
        auto guage_pressure =
            std::clamp<double>(m.pressure_setpoint, ATM_PRESSURE_MBAR * -1, 0);
        auto target_pressure = guage_pressure + p_atm;

        // Check for Significant Target Change (> 10% of the vacuum range)
        auto current_pressure = _control_state.pressure_abs_b;
        auto current_target = _control_state.target_pressure;
        auto vacuum_depth = std::abs(p_atm - target_pressure);
        auto change_delta = std::abs(target_pressure - current_target);

        // If the target moves by more than N % of the intended depth,
        // the previous baseline is no longer physically representative.
        if (_detector.baseline_captured() &&
            (change_delta >
             (vacuum_depth * waste_detector::BASELINE_DEPTH_RESET))) {
            _detector.reset_baseline();
        }

        _control_state.target_pressure = target_pressure;
        _control_state.duration_s = m.duration_s;
        _control_state.timeout_s = m.timeout_s;
        _control_state.vent_after = m.vent_after;
        _control_state.target_pressure_reached = false;

        // Start the duration timer
        if (m.start_pump && m.duration_s > 0) {
            _vacuum_timer.stop();
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            _vacuum_timer.update_period(m.duration_s * 1000);
            _vacuum_timer.start();
        }

        // Start the pressure control loop
        if (!_control_state.enable_vacuum && m.start_pump) {
            _control_state.control_pump = true;
            reset_pressure_state_buffer();
            _detector.reset();
            _controller.configure_slew(current_pressure, m.ramp_rate);

            // Start pressure control messages
            policy.start_pressure_control(true);
        }
        _control_state.enable_vacuum = m.start_pump;

        send_ack_message(m.id);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::NotifyPumpRunMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        // PumpTask received direct SetPump (from_host); enable our
        // monitoring (periodic pressure reads, waste detection, duration
        // timer, target monitoring) so PumpTask can "use" this
        // functionality. We must not send any SetPumpState control messages
        // back to PumpTask.
        // Convert target guage presure to abs pressure
        if (m.run_pump) {
            update_pressures(policy, true);

            // Calculate target pressure as a percent
            auto p_atm = _control_state.pressure_atm;
            auto p_percent = std::clamp<int>(m.pressure_percent, 0, 100.0);
            _control_state.target_pressure = p_atm * p_percent / 100.0;

            _control_state.duration_s = m.duration_s;
            _control_state.timeout_s = m.timeout_s;
            _control_state.vent_after = m.vent_after;
            _control_state.target_pressure_reached = false;
            _control_state.control_pump = false;

            if (!_control_state.enable_vacuum) {
                reset_pressure_state_buffer();
                _detector.reset();
                policy.start_pressure_control(true);
            }

            if (m.duration_s > 0) {
                _vacuum_timer.stop();
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                _vacuum_timer.update_period(m.duration_s * 1000);
                _vacuum_timer.start();
            }
            // No ack sent here; the originating SetPump from host was already
            // acked by PumpTask.
        }
        _control_state.enable_vacuum = m.run_pump;
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::GetPressureStateMessage& m,
                       Policy& policy) -> void {
        // refresh if the pump is not running
        if (!_control_state.enable_vacuum) {
            for (auto sensor_id :
                 {ABS_PRESSURE_A, ABS_PRESSURE_B, ATM_PRESSURE}) {
                auto ret = update_pressure(sensor_id);
                // Reset the sensor if there is some problem
                if (ret != NO_ERROR) {
                    policy.sensor_reset(sensor_id);
                    continue;
                }
            }
        }

        // Convert to guage pressure
        auto current_pressure =
            _control_state.pressure_abs_b - _control_state.pressure_atm;
        auto target_pressure =
            _control_state.target_pressure != 0
                ? _control_state.target_pressure - _control_state.pressure_atm
                : 0;
        auto msg = messages::GetPressureStateResponseMessage{
            .responding_to_id = m.id,
            .target_pressure = target_pressure,
            .current_pressure = current_pressure,
            .pressure_abs_a = _control_state.pressure_abs_a,
            .pressure_abs_b = _control_state.pressure_abs_b,
            .pressure_atm = _control_state.pressure_atm,
            .vacuum_enabled = _control_state.enable_vacuum,
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            .duration_s = _vacuum_timer.get_remaining_time() / 1000,
            .vent_state = _control_state.vent_state,
        };
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::HostCommsAddress));
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetVentMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto state = static_cast<VentState>(m.state);
        auto ok = set_vent_state(state);
        auto ret = ok ? Error::NO_ERROR : Error::VENT_FAILED_ERROR;
        send_ack_message(m.id, ret);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetPressurePIDMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto tol = m.rel_tol_pct.value_or(_control_state.rel_tol_pct);
        _control_state.rel_tol_pct =
            std::clamp(tol > 0 ? tol : REL_PRESSURE_TOLERANCE_PCT, 0.0, 100.0);
        auto cs = _controller.get_state();
        auto kp = m.kp.value_or(cs.kp);
        auto ki = m.ki.value_or(cs.ki);
        auto kd = m.kd.value_or(cs.kd);
        auto overshoot = m.overshoot.value_or(cs.overshoot);
        auto k_velocity = m.k_velocity.value_or(cs.k_velocity);
        auto k_holding = m.k_holding.value_or(cs.k_holding);
        _controller.configure_pid(kp, ki, kd, k_velocity, k_holding, overshoot,
                                  m.reset);

        send_ack_message(m.id);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::GetPressurePIDMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto cs = _controller.get_state();
        auto msg = messages::GetPressurePIDResponseMessage{
            .responding_to_id = m.id,
            .kp = cs.kp,
            .ki = cs.ki,
            .kd = cs.kd,
            .overshoot = cs.overshoot,
            .k_velocity = cs.k_velocity,
            .k_holding = cs.k_holding,
            .rel_tol_pct = _control_state.rel_tol_pct};
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::HostCommsAddress));
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetWasteDetectionConfigMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        auto dc = _detector.get_config();
        dc.enable_waste_full = m.enable_waste_full;
        dc.p_window_start = m.p_window_start.value_or(dc.p_window_start);
        dc.p_window_end = m.p_window_end.value_or(dc.p_window_end);
        dc.baseline_fast_factor =
            m.baseline_fast_factor.value_or(dc.baseline_fast_factor);
        dc.max_delta_per_tick =
            m.max_delta_per_tick.value_or(dc.max_delta_per_tick);
        dc.max_rise_per_tick =
            m.max_rise_per_tick.value_or(dc.max_rise_per_tick);
        dc.max_cummulative_rise =
            m.max_cummulative_rise.value_or(dc.max_cummulative_rise);
        dc.p_filter_alpha = m.p_filter_alpha.value_or(dc.p_filter_alpha);
        dc.min_window_time = m.min_window_time.value_or(dc.min_window_time);
        dc.max_window_time = m.max_window_time.value_or(dc.max_window_time);
        _detector.configure(dc);
        send_ack_message(m.id);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::GetWasteDetectionConfigMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        auto dc = _detector.get_config();
        auto msg = messages::GetWasteDetectionConfigResponse{
            .responding_to_id = m.id,
            .enable_waste_full = dc.enable_waste_full,
            .p_window_start = dc.p_window_start,
            .p_window_end = dc.p_window_end,
            .baseline_fast_factor = dc.baseline_fast_factor,
            .max_delta_per_tick = dc.max_delta_per_tick,
            .max_rise_per_tick = dc.max_rise_per_tick,
            .max_cummulative_rise = dc.max_cummulative_rise,
            .p_filter_alpha = dc.p_filter_alpha,
            .min_window_time = dc.min_window_time,
            .max_window_time = dc.max_window_time};
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::HostCommsAddress));
    }

    auto get_sensor(PressureSensorID sensor_id) -> PressureSensor& {
        switch (sensor_id) {
            case ABS_PRESSURE_A:
                return _abs_pressure_a;
            case ABS_PRESSURE_B:
                return _abs_pressure_b;
            case ATM_PRESSURE:
                return _atm_pressure;
            default:
                return _abs_pressure_a;
        }
    }

    template <PressureControlPolicy Policy>
    auto update_pressures(Policy& policy, bool reset_filter = false) -> void {
        for (auto sensor_id : {ABS_PRESSURE_A, ABS_PRESSURE_B, ATM_PRESSURE}) {
            auto ret = update_pressure(sensor_id, reset_filter);
            // Reset the sensor if there is some problem
            if (ret != NO_ERROR) {
                policy.sensor_reset(sensor_id);
                continue;
            }
        }
    }

    auto update_pressure(PressureSensorID sensor_id, bool reset_filter = false)
        -> PressureSensorError {
        auto& sensor = get_sensor(sensor_id);
        if (!sensor.ok) {
            return DRIVER_INIT_ERROR;
        }

        auto pressure = std::visit(
            [&](auto&& driver) -> double {
                return driver.read_pressure(reset_filter);
            },
            sensor.driver);

        // TODO: Handle error
        if (pressure < 0) {
            // TODO: Maybe return specific driver error
            return MATH_SATURATION_ERROR;
        }

        if (sensor_id == ABS_PRESSURE_A) {
            _control_state.pressure_abs_a = pressure;
        } else if (sensor_id == ABS_PRESSURE_B) {
            _control_state.pressure_abs_b = pressure;
        } else if (sensor_id == ATM_PRESSURE) {
            _control_state.pressure_atm = pressure;
        }

        return NO_ERROR;
    }

    auto set_vent_state(VentState set_state) -> bool {
        // Use the higher run current when switching states
        // Use the lower hold current when holding state
        _policy->set_vent_voltage(VENT_RUN_VOLT);
        _policy->set_vent_state(set_state);
        PressurePolicy::sleep_ms(VENT_ACTUATE_DELAY);
        _policy->set_vent_voltage(VENT_HOLD_VOLT);

        auto vent_state = _policy->get_vent_state();
        _control_state.vent_state = vent_state;
        return vent_state == set_state;
    }

    auto set_pump_state(bool run_pump, double rpm = 0.0) -> void {
        auto cs = _controller.get_state();
        auto pump_ramp = std::clamp(cs.ramp_rate * cs.k_velocity, PUMP_RAMP_MIN,
                                    PUMP_RAMP_MAX);
        auto msg = messages::SetPumpStateMessage{.rpm_setpoint = rpm,
                                                 .run_pump = run_pump,
                                                 .ramp_rate = pump_ramp};
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::PumpAddress));
    }

    auto monitor_target_pressure() -> void {
        if (!_control_state.target_pressure_reached) {
            // Pressure has not been reached within the given time
            auto elapsed_time_s =
                (_control_state.last_tick - _control_state.start_time_ms) *
                MS_TO_SECONDS;
            if (_control_state.timeout_s > 0 &&
                elapsed_time_s > _control_state.timeout_s &&
                _control_state.vent_state == VentState::CLOSED) {
                // we've reached the end of the allowed time to reach target
                // pressure while the vent was closed.
                _control_state.error = Error::PRESSURE_NOT_REACHED_ERROR;
                _control_state.enable_vacuum = false;
                return;
            }
            _control_state.target_pressure_reached =
                maintaining_target_pressure();
        }
        // if we wanted to keep checking during the hold time that the pressure
        // holds, we could do it here
    }

    auto vacuum_timer_end_callback() -> void {
        _vacuum_timer.stop();
        _control_state.enable_vacuum = false;
    }

    auto stop_vacuum() -> void {
        _vacuum_timer.stop();
        _policy->start_pressure_control(false);
        set_pump_state(false, 0);

        _detector.reset();
        _controller.reset();
        _control_state.current_pressure = 0;
        _control_state.target_pressure = 0;
        _control_state.target_rpm = 0;
        _control_state.last_tick = 0;
        _control_state.start_time_ms = 0;
        _control_state.duration_s = 0;
        _control_state.timeout_s = 0;
        _control_state.enable_vacuum = false;
        _control_state.target_pressure_reached = false;
        _control_state.control_pump = true;
    }

    auto maintaining_target_pressure() -> bool {
        if (pressure_state_buffer_index == 0) {
            return false;
        }

        // Calculate effective tolerance:
        auto target_abs = _control_state.target_pressure;
        auto vacuum_depth = _control_state.pressure_atm - target_abs;
        auto rel_tol = vacuum_depth * (_control_state.rel_tol_pct / 100.0);
        auto effective_tol = std::max(rel_tol, MIN_PRESSURE_TOLERANCE_MBAR);

        for (int i = 0; i < PRESSURE_STATE_BUFFER_LEN; ++i) {
            auto stored_abs = pressure_state_buffer.at(i);
            if (std::abs(stored_abs - target_abs) > effective_tol) {
                return false;
            }
        }
        return true;
    }

    auto reset_pressure_state_buffer() -> void {
        pressure_state_buffer.fill(0);
    }

    auto handle_control_state_outcomes() -> void {
        if (!_control_state.enable_vacuum) {
            stop_vacuum();
        }

        // Set the final vent state
        auto state = static_cast<VentState>(_control_state.vent_after);
        set_vent_state(state);

        // Report any errors
        if (_control_state.error != Error::NO_ERROR) {
            send_error_message(_control_state.error);
            _control_state.error = Error::NO_ERROR;
        }
    }

    auto send_debug_message(const char* message) -> void {
        if (_task_registry) {
            std::strcpy(DEBUG_BUF.data(), message);
            auto msg = messages::DebugMessage{.message = DEBUG_BUF};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

    auto send_error_message(Error error) -> void {
        if (_task_registry) {
            auto msg = messages::ErrorMessage{.code = error};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

    auto send_ack_message(uint32_t response_id, Error error = Error::NO_ERROR)
        -> void {
        if (_task_registry) {
            auto msg = messages::AcknowledgePrevious{
                .responding_to_id = response_id, .with_error = error};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    PressurePolicy* _policy;
    bool _initialized{false};
    FreeRTOSTimer _vacuum_timer;

    PressureSensor _abs_pressure_a = abs_pressure_a;
    PressureSensor _abs_pressure_b = abs_pressure_b;
    PressureSensor _atm_pressure = atm_pressure;
    std::array<double, PRESSURE_STATE_BUFFER_LEN> pressure_state_buffer = {0};
    uint8_t pressure_state_buffer_index = 0;

    PressureControlState _control_state = pressure_control_state;
    PressureController _controller;
    WasteDetector _detector;
};

}  // namespace pressure_task
