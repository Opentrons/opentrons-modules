/*
 * the primary interface to the motor control task
 */
#pragma once

#include <algorithm>
#include <concepts>
#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace motor_task {

/*
 * The MotorExecutionPolicy is how the portable task interacts
 * with the hardware. It is defined as a concept so it can be
 * passed as a reference paramter to run_once(), which means the
 * type of policy in actual use does not have to be part of the class's
 * type signature (which is used all over the place), just run_once's
 * type signature, which is used just by the rtos task and the test
 * harness.
 *
 * The policy exposes methods to get relevant data from the motor hardware
 * and methods to change the state of the motor controller.
 *
 * The policy is not the only way in which the hardware may interact
 * with the motor controller; it may also send messages. This should
 * be the way that the hardware sends information to the motor task
 * (as opposed to the motor task querying information from the hardware).
 * For instance, an asynchronous error mechanism should inform the motor
 * task of its event by sending a message.
 */
template <typename Policy>
concept MotorExecutionPolicy = requires(Policy& p, const Policy& cp) {
    { p.set_rpm(static_cast<int16_t>(16)) } -> std::same_as<errors::ErrorCode>;
    { cp.get_current_rpm() } -> std::same_as<int16_t>;
    { cp.get_target_rpm() } -> std::same_as<int16_t>;
    {p.stop()};
    {
        p.set_ramp_rate(static_cast<int32_t>(8))
        } -> std::same_as<errors::ErrorCode>;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.set_pid_constants(1.0, 2.0, 3.0)};
    {p.homing_solenoid_disengage()};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.homing_solenoid_engage(122)};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.delay_ticks(10)};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.plate_lock_set_power(0.1)};
    {p.plate_lock_disable()};
};

struct State {
    enum TaskStatus {
        STOPPED_UNKNOWN,  // Stopped but unclear whether we're homed (how we
                          // boot)
        RUNNING,          // Running under a speed control or ramping (including
                          // speed=0)
        ERROR,            // In an error state from the motor driver
        HOMING_MOVING_TO_HOME_SPEED,  // Heading towards an appropriate speed
                                      // for homing
        HOMING_COASTING_TO_STOP,  // solenoid engaged, waiting for it to fall
                                  // home
        STOPPED_HOMED             // Stopped and definitely homed
    };
    TaskStatus status;
};

struct PlateLockState {
    enum PlateLockTaskStatus {
        IDLE_CLOSED = 0,
        OPENING = 1,
        IDLE_OPEN = 2,
        CLOSING = 3,
        IDLE_UNKNOWN = 4
    };
    PlateLockTaskStatus status;
};

constexpr size_t RESPONSE_LENGTH = 128;
using Message = ::messages::MotorMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class MotorTask {
    static constexpr const uint32_t HOMING_INTERSTATE_WAIT_TICKS = 100;
    static constexpr const uint16_t PLATE_LOCK_WAIT_TICKS = 100;
    static constexpr const uint16_t STARTUP_HOMING_WAIT_TICKS =
        200;  // needed to ensure motor setup complete at startup before homing
    static constexpr const uint16_t MOTOR_START_WAIT_TICKS = 1000;
    static constexpr const uint16_t POST_HOMING_WAIT_TICKS =
        500;  // needed to ensure motor control deactivated before subsequent
              // SetRPM commands

  public:
    static constexpr int16_t HOMING_ROTATION_LIMIT_HIGH_RPM = 250;
    static constexpr int16_t HOMING_ROTATION_LIMIT_LOW_RPM = 200;
    static constexpr int16_t HOMING_ROTATION_LOW_MARGIN = 25;
    static constexpr uint16_t HOMING_SOLENOID_CURRENT_INITIAL = 200;
    static constexpr uint16_t HOMING_SOLENOID_CURRENT_HOLD = 75;
    static constexpr uint16_t HOMING_CYCLES_BEFORE_TIMEOUT = 10;
    static constexpr uint16_t PLATE_LOCK_MOVE_TIME_THRESHOLD =
        4950;  // 1250 for 380:1 motor, 2350 for 1000:1 motor. Updated to 4950
               // for SZ testing, needs to be tuned down (must end in 50 to pass
               // tests)
    static constexpr int16_t MOTOR_START_THRESHOLD_RPM = 20;
    using Queue = QueueImpl<Message>;
    static constexpr uint8_t PLATE_LOCK_STATE_SIZE = 14;
    explicit MotorTask(Queue& q)
        : state{.status = State::STOPPED_UNKNOWN},
          plate_lock_state{.status = PlateLockState::IDLE_UNKNOWN},
          message_queue(q),
          task_registry(nullptr),
          setpoint(0) {}
    MotorTask(const MotorTask& other) = delete;
    auto operator=(const MotorTask& other) -> MotorTask& = delete;
    MotorTask(MotorTask&& other) noexcept = delete;
    auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
    ~MotorTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    [[nodiscard]] auto get_state() const -> State::TaskStatus {
        return state.status;
    }
    [[nodiscard]] auto get_plate_lock_state() const
        -> PlateLockState::PlateLockTaskStatus {
        return plate_lock_state.status;
    }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());

        // This is the call down to the provided queue. It will block for
        // anywhere up to the provided timeout, which drives the controller
        // frequency.

        static_cast<void>(message_queue.recv(&message));
        std::visit(
            [this, &policy](const auto& msg) -> void {
                this->visit_message(msg, policy);
            },
            message);
    }

  private:
    template <typename Policy>
    auto visit_message(const std::monostate& _ignore, Policy& policy) -> void {
        static_cast<void>(_ignore);
        static_cast<void>(policy);
    }

    template <typename Policy>
    auto visit_message(const messages::SetRPMMessage& msg, Policy& policy)
        -> void {
        auto error = errors::ErrorCode::NO_ERROR;
        if ((!policy.plate_lock_closed_sensor_read()) &&
            (plate_lock_state.status != PlateLockState::IDLE_CLOSED)) {
            error = errors::ErrorCode::PLATE_LOCK_NOT_CLOSED;
        } else {
            policy.homing_solenoid_disengage();
            error = policy.set_rpm(msg.target_rpm);
            if (error == errors::ErrorCode::NO_ERROR) {  // only proceed if
                                                         // target speed legal
                setpoint = msg.target_rpm;
                state.status = State::RUNNING;
                policy.delay_ticks(MOTOR_START_WAIT_TICKS);
                if ((msg.target_rpm != 0) &&
                    (policy.get_current_rpm() < MOTOR_START_THRESHOLD_RPM)) {
                    error = errors::ErrorCode::MOTOR_UNABLE_TO_MOVE;
                    policy.stop();
                    state.status = State::ERROR;
                    setpoint = 0;
                }
            }
        }
        if (current_error !=
            errors::ErrorCode::NO_ERROR) {  // motor-control error supercedes
                                            // illegal-speed and unable-to-move
                                            // errors
            error = current_error;
        }
        auto response = messages::AcknowledgePrevious{
            .responding_to_id = msg.id, .with_error = error};
        if (msg.from_system) {
            static_cast<void>(
                task_registry->system->get_message_queue().try_send(
                    messages::SystemMessage(response)));
        } else {
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <typename Policy>
    auto visit_message(const messages::SetPIDConstantsMessage& msg,
                       Policy& policy) -> void {
        policy.set_pid_constants(msg.kp, msg.ki, msg.kd);
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::AcknowledgePrevious{.responding_to_id = msg.id}));
    }

    template <typename Policy>
    auto visit_message(const messages::SetAccelerationMessage& msg,
                       Policy& policy) -> void {
        auto error = policy.set_ramp_rate(msg.rpm_per_s);
        auto response = messages::AcknowledgePrevious{
            .responding_to_id = msg.id, .with_error = error};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::GetRPMMessage& msg, Policy& policy)
        -> void {
        auto response =
            messages::GetRPMResponse{.responding_to_id = msg.id,
                                     .current_rpm = policy.get_current_rpm(),
                                     .setpoint_rpm = setpoint};
        if (state.status == State::ERROR) {
            response.with_error = current_error;
        }
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::MotorSystemErrorMessage& msg,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        if (msg.errors == 0) {
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(messages::ErrorMessage{
                        .code = errors::ErrorCode::MOTOR_SPURIOUS_ERROR})));
            return;
        }
        if (state.status == State::HOMING_COASTING_TO_STOP) {
            policy.homing_solenoid_engage(HOMING_SOLENOID_CURRENT_HOLD);
            policy.stop();
            state.status = State::STOPPED_HOMED;
            setpoint = 0;
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::AcknowledgePrevious{.responding_to_id =
                                                      cached_home_id}));
        } else {
            for (auto offset = static_cast<uint8_t>(
                     errors::MotorErrorOffset::FOC_DURATION);
                 offset <=
                 static_cast<uint8_t>(errors::MotorErrorOffset::SW_ERROR);
                 offset++) {
                auto code = errors::from_motor_error(
                    msg.errors, static_cast<errors::MotorErrorOffset>(offset));
                if (code != errors::ErrorCode::NO_ERROR) {
                    auto message = messages::UpdateLEDStateMessage{
                        .color = LED_COLOR::AMBER, .mode = LED_MODE::PULSE};
                    static_cast<void>(
                        task_registry->system->get_message_queue().try_send(
                            message));
                    policy.stop();
                    state.status = State::ERROR;
                    setpoint = 0;
                    current_error = code;
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            messages::HostCommsMessage(
                                messages::ErrorMessage{.code = code})));
                }
            }
        }
    }

    /**
     * CheckHomingStatusMessage and BeginHomingMessage are the two main
     * components of the home sequence state machine. This task is designed to
     * react to messages, which means it really doesn't want to wait forever
     * doing complex tasks - it wants to do something quick and exit to handle
     * more messages. For something like the homing state machine, though, we
     * have some possibly-long-running sequences, like
     * - Set low speed
     * - wait until that happens
     * - set solenoid
     * - wait until the motor driver says we stalled or for a period of time
     *
     * So we replace any wait states with repeatedly sending ourselves another
     * CheckHomingStatusMessage. Because we talk with queues, we won't spinlock
     * ourselves - any messages sent asynchronously will get enqueued and
     * handled eventually, and we'll wait a bit always in between runs - but we
     * still do a bit of a sleep because otherwise we'd run every tick.
     *
     * So, the sequence is
     * - Get a BeginHomingMessage and take the quick actions of setting an RPM
     * target and doublechecking the solenoid is disengaged, then send ourselves
     * a check-status
     * - When we get a check-status, go from moving-to-speed to coasting-to-stop
     * if we can and otherwise send another check-status
     * - When in coasting-to-stop, keep sending those check-statuses. If we keep
     * the solenoid engaged forever, it will fry itself, so we need a timeout.
     * In either case, we've probably homed successfully; sadly, the motor
     * system isn't quite good enough to detect when it's homed on its own.
     * */

    template <typename Policy>
    auto visit_message(const messages::CheckHomingStatusMessage& msg,
                       Policy& policy) -> void {
        static_cast<void>(msg);
        if (state.status == State::HOMING_MOVING_TO_HOME_SPEED) {
            if (policy.get_current_rpm() < HOMING_ROTATION_LIMIT_HIGH_RPM &&
                policy.get_current_rpm() > HOMING_ROTATION_LIMIT_LOW_RPM) {
                policy.homing_solenoid_engage(HOMING_SOLENOID_CURRENT_INITIAL);
                state.status = State::HOMING_COASTING_TO_STOP;
                homing_cycles_coasting = 0;
                policy.delay_ticks(HOMING_INTERSTATE_WAIT_TICKS);
            }
            policy.delay_ticks(HOMING_INTERSTATE_WAIT_TICKS);
            static_cast<void>(
                get_message_queue().try_send(messages::CheckHomingStatusMessage{
                    .from_startup = msg.from_startup}));
        } else if (state.status == State::HOMING_COASTING_TO_STOP) {
            homing_cycles_coasting++;
            if (homing_cycles_coasting > HOMING_CYCLES_BEFORE_TIMEOUT) {
                policy.homing_solenoid_engage(HOMING_SOLENOID_CURRENT_HOLD);
                policy.stop();
                state.status = State::STOPPED_HOMED;
                setpoint = 0;
                policy.delay_ticks(POST_HOMING_WAIT_TICKS);
                if (!msg.from_startup) {
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            messages::AcknowledgePrevious{.responding_to_id =
                                                              cached_home_id}));
                }
            } else {
                policy.delay_ticks(HOMING_INTERSTATE_WAIT_TICKS);
                static_cast<void>(get_message_queue().try_send(
                    messages::CheckHomingStatusMessage{.from_startup =
                                                           msg.from_startup}));
            }
        }
    }

    template <typename Policy>
    auto visit_message(const messages::BeginHomingMessage& msg, Policy& policy)
        -> void {
        if ((!policy.plate_lock_closed_sensor_read()) &&
            (plate_lock_state.status != PlateLockState::IDLE_CLOSED)) {
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::AcknowledgePrevious{
                        .responding_to_id = msg.id,
                        .with_error =
                            errors::ErrorCode::PLATE_LOCK_NOT_CLOSED}));
        } else {
            state.status = State::HOMING_MOVING_TO_HOME_SPEED;
            policy.homing_solenoid_disengage();
            policy.set_rpm(HOMING_ROTATION_LIMIT_LOW_RPM +
                           HOMING_ROTATION_LOW_MARGIN);
            policy.delay_ticks(MOTOR_START_WAIT_TICKS);
            cached_home_id = msg.id;
            if (policy.get_current_rpm() < MOTOR_START_THRESHOLD_RPM) {
                auto error = errors::ErrorCode::MOTOR_UNABLE_TO_MOVE;
                policy.stop();
                state.status = State::ERROR;
                setpoint = 0;
                if (msg.from_startup) {
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            messages::ErrorMessage{.code = error}));
                } else {
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            messages::AcknowledgePrevious{
                                .responding_to_id = cached_home_id,
                                .with_error = error}));
                }
            } else {
                static_cast<void>(get_message_queue().try_send(
                    messages::CheckHomingStatusMessage{.from_startup =
                                                           msg.from_startup}));
            }
        }
    }

    template <typename Policy>
    auto visit_message(const messages::ActuateSolenoidMessage& msg,
                       Policy& policy) -> void {
        state.status = State::STOPPED_UNKNOWN;
        if (msg.current_ma == 0) {
            policy.homing_solenoid_disengage();
        } else {
            policy.homing_solenoid_engage(static_cast<double>(msg.current_ma));
        }
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::SetPlateLockPowerMessage& msg,
                       Policy& policy) -> void {
        if (msg.power == 0) {
            policy.plate_lock_disable();
            plate_lock_state.status = PlateLockState::IDLE_UNKNOWN;
        } else {
            policy.plate_lock_set_power(std::clamp(msg.power, -1.0F, 1.0F));
            if (msg.power < 0) {
                plate_lock_state.status = PlateLockState::OPENING;
            } else if (msg.power > 0) {
                plate_lock_state.status = PlateLockState::CLOSING;
            }
        }
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::AcknowledgePrevious{.responding_to_id = msg.id}));
    }

    template <typename Policy>
    auto visit_message(const messages::OpenPlateLockMessage& msg,
                       Policy& policy) -> void {
        static constexpr float OpenPower = 1.0F;
        auto check_state_message =
            messages::CheckPlateLockStatusMessage{.responding_to_id = msg.id};
        if ((policy.plate_lock_open_sensor_read()) ||
            (plate_lock_state.status == PlateLockState::IDLE_OPEN)) {
            plate_lock_state.status = PlateLockState::IDLE_OPEN;
        } else {
            if (state.status != State::STOPPED_HOMED) {
                check_state_message.with_error =
                    errors::ErrorCode::MOTOR_NOT_HOME;
            } else {
                policy.plate_lock_set_power(OpenPower);
                plate_lock_state.status = PlateLockState::OPENING;
                polling_time = 0;
            }
        }
        static_cast<void>(get_message_queue().try_send(check_state_message));
    }

    template <typename Policy>
    auto visit_message(const messages::ClosePlateLockMessage& msg,
                       Policy& policy) -> void {
        static constexpr float ClosePower = -1.0F;
        auto check_state_message = messages::CheckPlateLockStatusMessage{
            .responding_to_id = msg.id, .from_startup = msg.from_startup};
        if ((policy.plate_lock_closed_sensor_read()) ||
            (plate_lock_state.status == PlateLockState::IDLE_CLOSED)) {
            plate_lock_state.status = PlateLockState::IDLE_CLOSED;
        } else {
            if ((state.status != State::STOPPED_HOMED) &&
                (state.status != State::STOPPED_UNKNOWN)) {
                check_state_message.with_error =
                    errors::ErrorCode::MOTOR_NOT_STOPPED;
            } else {
                policy.plate_lock_set_power(ClosePower);
                plate_lock_state.status = PlateLockState::CLOSING;
                polling_time = 0;
            }
        }
        static_cast<void>(get_message_queue().try_send(check_state_message));
    }

    template <typename Policy>
    auto visit_message(const messages::CheckPlateLockStatusMessage& msg,
                       Policy& policy) -> void {
        if (msg.with_error != errors::ErrorCode::NO_ERROR) {
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::AcknowledgePrevious{
                        .responding_to_id = msg.responding_to_id,
                        .with_error = msg.with_error}));
        } else if ((plate_lock_state.status == PlateLockState::IDLE_CLOSED) ||
                   (plate_lock_state.status == PlateLockState::IDLE_OPEN)) {
            if (msg.from_startup) {
                policy.delay_ticks(STARTUP_HOMING_WAIT_TICKS);
                static_cast<void>(
                    get_message_queue().try_send(messages::BeginHomingMessage{
                        .from_startup = msg.from_startup}));
            } else {
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        messages::AcknowledgePrevious{
                            .responding_to_id = msg.responding_to_id}));
            }
        } else if (polling_time > PLATE_LOCK_MOVE_TIME_THRESHOLD) {
            policy.plate_lock_brake();
            plate_lock_state.status = PlateLockState::IDLE_UNKNOWN;
            if (msg.from_startup) {
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        messages::ErrorMessage{
                            .code = errors::ErrorCode::PLATE_LOCK_TIMEOUT}));
            } else {
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        messages::AcknowledgePrevious{
                            .responding_to_id = msg.responding_to_id,
                            .with_error =
                                errors::ErrorCode::PLATE_LOCK_TIMEOUT}));
            }
        } else {
            policy.delay_ticks(PLATE_LOCK_WAIT_TICKS);
            polling_time += PLATE_LOCK_WAIT_TICKS;
            static_cast<void>(get_message_queue().try_send(
                messages::CheckPlateLockStatusMessage{
                    .responding_to_id = msg.responding_to_id,
                    .from_startup = msg.from_startup}));
        }
    }

    template <typename Policy>
    auto visit_message(const messages::PlateLockComplete& msg, Policy& policy)
        -> void {
        policy.plate_lock_brake();
        if ((msg.closed == true) && (msg.open == false)) {
            plate_lock_state.status = PlateLockState::IDLE_CLOSED;
        } else if ((msg.open == true) && (msg.closed == false)) {
            plate_lock_state.status = PlateLockState::IDLE_OPEN;
        }
    }

    template <typename Policy>
    auto visit_message(const messages::GetPlateLockStateMessage& msg,
                       Policy& policy) -> void {
        std::array<char, 14> plate_lock_state_array = {};
        switch (plate_lock_state.status) {
            case PlateLockState::IDLE_CLOSED:
                plate_lock_state_array = std::array<char, 14>{"IDLE_CLOSED"};
                break;
            case PlateLockState::OPENING:
                plate_lock_state_array = std::array<char, 14>{"OPENING"};
                break;
            case PlateLockState::IDLE_OPEN:
                plate_lock_state_array = std::array<char, 14>{"IDLE_OPEN"};
                break;
            case PlateLockState::CLOSING:
                plate_lock_state_array = std::array<char, 14>{"CLOSING"};
                break;
            case PlateLockState::IDLE_UNKNOWN:
                plate_lock_state_array = std::array<char, 14>{"IDLE_UNKNOWN"};
                break;
            default:
                plate_lock_state_array = std::array<char, 14>{"IDLE_UNKNOWN"};
        }
        auto response = messages::GetPlateLockStateResponse{
            .responding_to_id = msg.id,
            .plate_lock_state = plate_lock_state_array};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::GetPlateLockStateDebugMessage& msg,
                       Policy& policy) -> void {
        // read each optical switch state
        bool open_switch = policy.plate_lock_open_sensor_read();
        bool closed_switch = policy.plate_lock_closed_sensor_read();

        std::array<char, 14> plate_lock_state_array = {};
        switch (plate_lock_state.status) {
            case PlateLockState::IDLE_CLOSED:
                plate_lock_state_array = std::array<char, 14>{"IDLE_CLOSED"};
                break;
            case PlateLockState::OPENING:
                plate_lock_state_array = std::array<char, 14>{"OPENING"};
                break;
            case PlateLockState::IDLE_OPEN:
                plate_lock_state_array = std::array<char, 14>{"IDLE_OPEN"};
                break;
            case PlateLockState::CLOSING:
                plate_lock_state_array = std::array<char, 14>{"CLOSING"};
                break;
            case PlateLockState::IDLE_UNKNOWN:
                plate_lock_state_array = std::array<char, 14>{"IDLE_UNKNOWN"};
                break;
            default:
                plate_lock_state_array = std::array<char, 14>{"IDLE_UNKNOWN"};
        }
        auto response = messages::GetPlateLockStateDebugResponse{
            .responding_to_id = msg.id,
            .plate_lock_state = plate_lock_state_array,
            .plate_lock_open_state = open_switch,
            .plate_lock_closed_state = closed_switch};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    State state;
    PlateLockState plate_lock_state;
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    uint32_t cached_home_id = 0;
    uint32_t homing_cycles_coasting = 0;
    uint32_t polling_time = 0;
    errors::ErrorCode current_error = errors::ErrorCode::NO_ERROR;
    int16_t setpoint;
};

};  // namespace motor_task
