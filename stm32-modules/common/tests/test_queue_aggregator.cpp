
#include <cstdint>
#include <variant>

#include "catch2/catch.hpp"
#include "core/queue_aggregator.hpp"
#include "test/test_message_queue.hpp"

// Some dummy message definitions
struct Message1 {
    uint32_t payload;
};

struct Message2 {
    uint32_t a, b;
};

struct Message3 {
    double a;
    size_t return_address;
};

// Some dummy message queue definitions
using Queue1 = TestMessageQueue<std::variant<Message1, Message2>, 0>;
using Queue2 = TestMessageQueue<std::variant<Message2, Message3>>;
// Clone of Queue1 with different index
using Queue3 = TestMessageQueue<std::variant<Message1, Message2>, 1>;

// Typedef of the aggregator

using Aggregator = queue_aggregator::QueueAggregator<Queue1, Queue2>;

// Indices for the queues
enum TaskIndex {
    Index1 = Aggregator::get_task_idx<Queue1>(),
    Index2 = Aggregator::get_task_idx<Queue2>()
};

TEST_CASE("queue aggregator registration and tag dispatching") {
    GIVEN("an uninitialized queue aggregator") {
        Queue1 q1("1");
        Queue2 q2("2");
        Aggregator aggregator;
        THEN("sending messsages fails") {
            REQUIRE(!aggregator.send(Message1{.payload = 5}, Queue1::Tag{}));
            REQUIRE(!aggregator.send(Message2{.a = 5, .b = 6}, Queue2::Tag{}));
            REQUIRE(!q1.has_message());
            REQUIRE(!q2.has_message());
        }
        AND_WHEN("populating the queue handles") {
            REQUIRE(aggregator.register_queue(q1));
            REQUIRE(aggregator.register_queue(q2));
            THEN("trying to re-register a queue returns an error") {
                REQUIRE(!aggregator.register_queue(q1));
            }
            THEN("sending messages with tag-dispatching succeeds") {
                REQUIRE(aggregator.send(Message1{.payload = 5}, Queue1::Tag{}));
                REQUIRE(
                    aggregator.send(Message2{.a = 5, .b = 6}, Queue2::Tag{}));
                REQUIRE(q1.has_message());
                REQUIRE(q2.has_message());
            }
        }
    }
}

TEST_CASE("queue aggregator message deduction") {
    GIVEN("an initialized queue aggregator") {
        Queue1 q1("1");
        Queue2 q2("2");
        Aggregator aggregator;
        REQUIRE(aggregator.register_queue(q1));
        REQUIRE(aggregator.register_queue(q2));
        WHEN("sending a message unique to Queue1") {
            Message1 msg{.payload = 5};
            REQUIRE(aggregator.send(msg));
            THEN("the message is received") {
                Queue1::Message received;
                REQUIRE(q1.try_recv(&received));
                REQUIRE(std::holds_alternative<Message1>(received));
                REQUIRE(std::get<Message1>(received).payload == 5);
            }
        }
        AND_GIVEN("a message ambiguous between queues") {
            Message2 msg{.a = 1, .b = 2};
            THEN("constructing a variant will disambiguate the recipient") {
                Queue2::Message received;
                Queue2::Message to_send(msg);
                REQUIRE(aggregator.send(to_send));
                REQUIRE(q2.try_recv(&received));
                REQUIRE(std::holds_alternative<Message2>(received));
                REQUIRE(std::get<Message2>(received).a == 1);
                REQUIRE(std::get<Message2>(received).b == 2);
            }
        }
    }
}

TEST_CASE("queue aggregator index-based sending") {
    GIVEN("an initialized queue aggregator") {
        Queue1 q1("1");
        Queue2 q2("2");
        Aggregator aggregator;
        REQUIRE(aggregator.register_queue(q1));
        REQUIRE(aggregator.register_queue(q2));
        GIVEN("a message with a return address") {
            Message3 message{
                .a = 5, .return_address = aggregator.get_task_idx<Queue1>()};
            WHEN("sending and receiving message") {
                REQUIRE(aggregator.send(message));
                REQUIRE(q2.has_message());
                Queue2::Message rcv;
                REQUIRE(q2.try_recv(&rcv));
                REQUIRE(std::holds_alternative<Message3>(rcv));
                auto received = std::get<Message3>(rcv);
                REQUIRE(received.a == 5);
                REQUIRE(received.return_address == 0);
                THEN("recipient can reply to the return address") {
                    Message2 reply{.a = 1, .b = 2};
                    REQUIRE(aggregator.send_to_address(
                        reply, received.return_address));
                    REQUIRE(q1.has_message());
                    Queue1::Message rcv_2;
                    REQUIRE(q1.try_recv(&rcv_2));
                    REQUIRE(std::holds_alternative<Message2>(rcv_2));
                    auto received_2 = std::get<Message2>(rcv_2);
                    REQUIRE(received_2.a == 1);
                    REQUIRE(received_2.b == 2);
                }
            }
        }
        GIVEN("a message shared by each queue type") {
            Message2 message;
            THEN("sending to both queues succeeds") {
                REQUIRE(aggregator.send_to_address(message, TaskIndex::Index1));
                REQUIRE(aggregator.send_to_address(message, TaskIndex::Index2));
                REQUIRE(q1.has_message());
                REQUIRE(q2.has_message());
            }
        }
        GIVEN("a message NOT shared by each queue type") {
            Message3 message;
            THEN("sending to the correct queue succeeds") {
                REQUIRE(aggregator.send_to_address(message, TaskIndex::Index2));
                REQUIRE(!q1.has_message());
                REQUIRE(q2.has_message());
            }
            THEN("sending to the wrong queue fails") {
                REQUIRE(
                    !aggregator.send_to_address(message, TaskIndex::Index1));
                REQUIRE(!q1.has_message());
                REQUIRE(!q2.has_message());
            }
        }
        WHEN("sending to an invalid address") {
            Message2 message;
            size_t address = 0xFFFF;
            THEN("sending fails") {
                REQUIRE(!aggregator.send_to_address(message, address));
            }
        }
    }
}

TEST_CASE("queue aggregator constructor") {
    GIVEN("queue handles") {
        Queue1 q1("1");
        Queue2 q2("2");
        WHEN("constructing aggregator with handles") {
            auto aggregator = queue_aggregator::QueueAggregator(q1, q2);
            THEN("the handles have been registered") {
                REQUIRE(aggregator.send(Message1{}));
                REQUIRE(q1.has_message());
                REQUIRE(aggregator.send(Message3{}));
                REQUIRE(q2.has_message());
            }
        }
    }
}

TEST_CASE("queues with identical message types") {
    GIVEN("queue handles with identical types") {
        Queue1 q1("1");
        Queue3 q3("3");
        auto aggregator = queue_aggregator::QueueAggregator(q1, q3);
        constexpr auto Index3 = aggregator.get_task_idx<Queue3>();
        THEN("index sending works") {
            Message2 message;
            REQUIRE(aggregator.send_to_address(message, TaskIndex::Index1));
            REQUIRE(q1.has_message());
            REQUIRE(!q3.has_message());
            REQUIRE(aggregator.send_to_address(message, Index3));
            REQUIRE(q3.has_message());
        }
    }
}
