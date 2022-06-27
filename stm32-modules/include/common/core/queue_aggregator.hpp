/**
 * @file queue_aggregator.hpp
 * @brief Provides a templated type to aggregate queues. See the description
 * of QueueAggregator for more detail.
 */

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <tuple>
#include <variant>

#include "hal/message_queue.hpp"

namespace queue_aggregator {

template <typename Q, typename Message = typename Q::Message, typename Tag = typename Q::Tag>
concept MsgQueue = MessageQueue<Q, Message>;

// In order to provide runtime visitation on the tuple of handles,
// we utilize this helper struct...
template <size_t N>
struct SendHelper {
    /**
     * @brief Helper function to provide runtime resolution of index-based
     * queue send operations. The function recurses down the range of indices
     * available for a QueueAggregator, and checks whether the runtime-
     * provided index `idx` is a match. If that is the case, a constexpr
     * evaluation checks wither the type `Message` can be used to construct
     * the message type we want to send to. If yes, the message is sent;
     * otherwise, we return `false` to indicate that the message and index
     * were mismatched.
     *
     * @tparam Message The message type to send
     * @tparam Aggregator Instantiation of the QueueAggregator class
     * @param msg The message to send
     * @param idx The queue index we want to send to; this should be derived
     *            from the `get_message_idx` or `get_queue_idx` function
     *            provided by QueueAggregator
     * @param handle Handle to the Aggregator
     * @return true if the messsage was succesfully sent, false otherwise
     */
    template <typename Message, typename Aggregator>
    static auto send(const Message& msg, size_t idx, Aggregator* handle)
        -> bool {
        using VariantType =
            std::variant_alternative_t<N - 1,
                                       typename Aggregator::MessageTypes>;
        if (N - 1 == idx) {
            if constexpr (std::is_constructible_v<VariantType, Message>) {
                return handle->template send_to<N - 1>(msg);
            }
            // Can't send to this queue type
            return false;
        }
            return SendHelper<N - 1>::send(msg, idx, handle);
    }
};

// Specialization for cases of N=0, in which case all functions return false
// to indicate a failure to resolve.
template <>
struct SendHelper<0> {
    template <typename Message, typename Aggregator>
    static auto send(const Message& /*unused*/, size_t /*unused*/, Aggregator* /*unused*/) -> bool {
        return false;
    }
};

/**
 * @brief Class to encapsulate a list of queues. This class functions as a
 * central mail-forwarding system in essence, redirecting messages to their
 * appropriate queues.
 *
 * @details
 * Each task on the system must provide its queue handle to the Aggregator,
 * as it has no forward declaration or knowledge of the full task types.
 * It is important that the tasks do not modify the memory address of
 * their queue handle (e.g. by reconstructing the queue), as the Aggregator
 * maintains links using pointers that last for the lifetime of the program.
 *
 * QueueAggregator provides a few mechanisms for sending messages:
 * - Tag Dispatching, wherein the caller provides both a message and a
 *   type of a "tag" struct that is associated with each message queue.
 * - Automatic resolution, wherein the caller can either:
 *    - Send a message that is unique to only one queue, in which case the
 *      recipient can be implicitly deduced
 *    - Send an instance of the actual \e variant used as the backing item
 *      in the intended queue, so long as that variant type is only used for
 *      one single message queue
 * - Index based resolution, where the address (index) of the recipient is
 *   passed as a runtime parameter and the QueueAggregator resolves the
 *   handle for the queue to send to & checks whether that queue can actually
 *   receive the Message type before sending it.
 *
 * One limitation of this configuration is that each message queue type
 * within MessageQueues must have a unique class definition. In the case
 * of two message queues with the same exact message type, this may be
 * as simple as providing a `size_t` template parameter for the queue
 * type, and providing a different value for each "version" of the queue.
 *
 * @tparam MessageQueues Parameter pack of the types of all of the message
 * queues. E.g. QueueAggregator<Queue1, Queue2, Queue3>
 */
template <MsgQueue... MessageQueues>
class QueueAggregator {
  private:
    using TagTypes = std::variant<typename MessageQueues::Tag...>;
    using MessageTypes = std::variant<typename MessageQueues::Message...>;

  public:
    static constexpr size_t TaskCount = sizeof...(MessageQueues);

    static_assert(TaskCount > 0, "Must have at least one queue");

    // Empty constructor
    QueueAggregator() : _handles() {}

    // Construct from a complete set of message queues. The handles are
    // passed as a reference rather than pointers in order to simplify type
    // deduction.
    QueueAggregator(MessageQueues&... handles) : _handles(&handles...) {}

    /**
     * @brief Register a queue handle with the task list
     *
     * @tparam Idx The index of this queue
     * @tparam Queue Type of the message queue
     * @param queue Handle to the queue
     * @return true if the queue handle could be registered, false
     * if it could not be registered
     */
    template <MsgQueue Queue>
    auto register_queue(Queue& queue) -> bool {
        constexpr auto idx = get_queue_idx<Queue>();
        static_assert(idx < TaskCount, "Must provide a valid queue type");
        if (check_initialized<Queue>()) {
            // Not allowed to re-register
            return false;
        }
        std::get<idx>(_handles)._handle = &queue;
        return true;
    }

    /**
     * @brief Get the index of a queue, as generated by the Task List
     *
     * @tparam Queue The type of the queue
     * @return size_t A unique index for this queue type
     */
    template <MsgQueue Queue>
    consteval static auto get_queue_idx() -> size_t {
        static_assert(std::is_class_v<typename Queue::Tag>);
        return get_tag_idx<typename Queue::Tag>();
    }

    /**
     * @brief Send a message and automatically deduce the mailbox
     * to forward it to.
     *
     * @tparam Tag The tag type of the queue to send to. This must be
     * provided by each queue type declared for the QueueAggregator.
     * @tparam Message The type of message to send
     * @param msg The message to send
     * @param tag An instance of the tag type
     * @return true if the message could be sent, false otherwise
     */
    template <typename Tag, typename Message>
    auto send(const Message& msg, const Tag& tag = (Tag())) -> bool {
        static_cast<void>(tag);
        constexpr auto idx = get_tag_idx<Tag>();
        return send_to<idx>(msg);
    }

    /**
     * @brief Send a message and automatically deduce the mailbox
     * to forward it to. This will fail if the type of the message
     * is not unique.
     *
     * @tparam Message The type of message to send
     * @param msg The message to send
     * @return true if the message could be sent, false otherwise
     */
    template <typename Message>
    auto send(const Message& msg) -> bool {
        constexpr auto idx = get_message_idx<Message>();
        return send_to<idx>(msg);
    }

    /**
     * @brief Send a queue message to an address, specified as an
     * index at runtime
     *
     * @tparam Message The type of the message to send
     * @param msg The message to send
     * @param address The index of the queue to send the message to.
     * This should be derived from `get_queue_idx`.
     * @return true if the message could be sent, false otherwise
     */
    template <typename Message>
    auto send_to_address(const Message& msg, size_t address) -> bool {
        if (!(address < TaskCount)) {
            return false;
        }
        return SendHelper<TaskCount>::send(msg, address, this);
    }

  private:
    /**
     * @brief Wrapper class for holding a pointer to a queue with
     * a default nullptr value
     *
     * @tparam Queue type of queue to use
     */
    template <typename Queue>
    struct QueueHandle {
        QueueHandle(Queue* ptr = nullptr) : _handle(ptr) {}
        // Let this have public visibility because it is only used internally
        // to the QueueAggregator
        // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
        Queue* _handle;
    };

    /**
     * @brief Internal function for sending a message
     *
     * @tparam Idx The index of the message to send to, as returned by
     *             \ref get_queue_idx()
     * @tparam Message The type of message to send
     * @param msg The message to send
     * @return true if the message could be sent, false otherwise
     */
    template <size_t Idx, typename Message>
    auto send_to(const Message& msg) -> bool {
        static_assert(Idx < TaskCount, "Invalid task index");
        if (std::get<Idx>(_handles)._handle == nullptr) {
            return false;
        }
        return std::get<Idx>(_handles)._handle->try_send(msg);
    }

    /**
     * @brief Get the index of a queue, based on an individual message
     * definition. This deduction will ONLY work if the message is unique
     * to a specific queue (i.e. it is not sent to multiple tasks)
     * @tparam Message The message type to get the index of
     * @return the index of Message
     */
    template <typename Message>
    consteval static auto get_message_idx() -> size_t {
        return MessageTypes((Message())).index();
    }

    /**
     * @brief Get the index of a Tag
     *
     * @tparam Tag The Tag type to get the index of
     * @return the index of Tag
     */
    template <typename Tag>
    consteval static auto get_tag_idx() -> size_t {
        static_assert(std::is_constructible_v<TagTypes, Tag>,
                      "Must pass a valid tag type");
        return TagTypes((Tag())).index();
    }

    /**
     * @brief Checks whether a queue has been registered yet
     *
     * @tparam Queue The type of the queue to check for
     * @return true if the queue has been registered, false if it is still
     *         uninitialized.
     */
    template <typename Queue>
    [[nodiscard]] auto check_initialized() const -> bool {
        return std::get<get_queue_idx<Queue>()>(_handles)._handle != nullptr;
    }

    // SendHelper uses the internal send_to function...
    template <size_t N>
    friend struct SendHelper;

    // Handle for each of the tasks
    std::tuple<QueueHandle<MessageQueues>...> _handles;
};

};  // namespace queue_aggregator
