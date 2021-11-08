/*
 * message_queue contains a concept defining a type-safe C++ template class
 * wrapping a thread-safe queue, which will be implemented in firmware as a
 * FreeRTOS queue and in tests as either a deque if the test does not use
 * threading, or something like a boost threadsafe queue if it does.
 */
#pragma once
#include <concepts>

template <class MQ, typename MessageType>
concept MessageQueue = requires(MQ mq, MessageType mt, const MQ cmq,
                                const MessageType cmt) {
    // Queues must have a try-send that takes a message instance by either
    // value, or const-ref, and may be callable with a second timeout argument.
    // If the timeout value is anything other than 0, it may block; whether or
    // not a timeout is specified, always check the return value.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    { mq.try_send(cmt, 10) } -> std::same_as<bool>;
    { mq.try_send(cmt) } -> std::same_as<bool>;
    // Queues must have a receive that takes a message instance by ptr
    {mq.recv(&mt)};
    // Queues must have a try-receive that takes a message instance by ptr for
    // filling in.
    { mq.try_recv(&mt) } -> std::same_as<bool>;
    // Queues must have a try-receive that takes a message instance by ptr for
    // filling in, and takes a timeout
    { mq.try_recv(&mt, 8) } -> std::same_as<bool>;

    // Queues must have a const method to check whether there are messages.
    { cmq.has_message() } -> std::same_as<bool>;
};
