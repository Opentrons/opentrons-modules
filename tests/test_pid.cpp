#include <limits>
#include <vector>

#include "catch2/catch.hpp"
#include "ot_utils/core/pid.hpp"

using namespace ot_utils::pid;

SCENARIO("PID controller") {
    GIVEN("a PID controller initialized with all 0 coeffs") {
        auto p = PID(0, 0, 0, 1);
        WHEN("calculating a value") {
            auto result = p.compute(12312.0);
            THEN("the result should be 0") { REQUIRE(result == 0.0); }
            AND_WHEN("calculating a subsequent value") {
                auto result2 = p.compute(221351.2);
                THEN("the result should remain 0") { REQUIRE(result2 == 0.0); }
            }
        }
    }
    GIVEN("a PID controller") {
        auto p = PID(1.0, 2.0, 3.0, 1.0, 4.0, -5.0);
        WHEN("querying the accessors for static data") {
            auto kp = p.kp();
            auto ki = p.ki();
            auto kd = p.kd();
            auto ts = p.sampletime();
            auto windup_high = p.windup_limit_high();
            auto windup_low = p.windup_limit_low();
            THEN("the queried values should match the ctor values") {
                REQUIRE(kp == 1.0);
                REQUIRE(ki == 2.0);
                REQUIRE(kd == 3.0);
                REQUIRE(windup_high == 4.0);
                REQUIRE(windup_low == -5.0);
                REQUIRE(ts == 1.0);
            }
        }
        WHEN("computing values") {
            p.compute(2.0);
            p.compute(3.0);
            THEN("the updated state should be correct") {
                REQUIRE(p.last_error() == 3.0);
                REQUIRE(p.last_iterm() == 4.0);
            }
        }
    }
    GIVEN("a PID controller with only kp") {
        auto p = PID(2.0, 0, 0, 1.0);
        WHEN("repeatedly calculating controls") {
            std::vector<float> results(8);
            std::vector<float> inputs = {0, 1, 2, 3, 4, 5, 6, 7};
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the result should depend only on the current input") {
                std::vector<float> intended(8);
                std::transform(inputs.cbegin(), inputs.cend(), intended.begin(),
                               [](float error) { return error * 2.0; });
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
        WHEN("calculating controls with a reset after every calculation") {
            std::vector<float> results(8);
            std::vector<float> inputs = {0, 1, 2, 3, 4, 5, 6, 7};
            std::transform(inputs.cbegin(), inputs.cend(), results.begin(),
                           [&p](const float& error) {
                               p.reset();
                               return p.compute(error);
                           });
            THEN("the result should depend only on the current input") {
                std::vector<float> intended(8);
                std::transform(inputs.cbegin(), inputs.cend(), intended.begin(),
                               [](float error) { return error * 2.0; });
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
        WHEN("calculating controls with varied sample times") {
            std::vector<float> results(5);
            std::vector<float> inputs = {0.1, 0.2, 0.3, 0.4, 0.5};
            std::transform(inputs.cbegin(), inputs.cend(), results.begin(),
                           [&p](const float& sampletime) {
                               return p.compute(1.0, sampletime);
                           });
            THEN("the sample time should not affect the output") {
                std::vector<float> intended(5);
                std::transform(inputs.cbegin(), inputs.cend(), intended.begin(),
                               [](float sampletime) { return 2.0; });
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
    }
    GIVEN("a PID controller with only kd") {
        auto p = PID(0, 0, 1.0, 1.0);
        WHEN("repeatedly calculating controls") {
            std::vector<float> results(8);
            std::vector<float> inputs = {0, 1, 2, 4, 8, 16, 32, 64};
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the result should depend only on the trailing difference") {
                std::vector<float> intended;
                intended.reserve(8);
                auto output_iter = intended.begin();
                auto trailing_input = inputs.cbegin();
                bool is_first = true;
                std::for_each(inputs.cbegin(), inputs.cend(),
                              [&output_iter, &trailing_input, &is_first,
                               &intended](float error) {
                                  if (is_first) {
                                      is_first = false;
                                      intended.push_back(error);
                                  } else {
                                      auto last = *trailing_input;
                                      trailing_input++;
                                      intended.push_back(error - last);
                                  }
                              });
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
        WHEN("resetting in between calculations") {
            std::vector<float> results(8);
            std::vector<float> inputs = {0, 1, 2, 4, 8, 16, 32, 64};
            std::transform(inputs.cbegin(), inputs.cend(), results.begin(),
                           [&p](const float& error) {
                               p.reset();
                               return p.compute(error);
                           });
            THEN("the result should always calculate a difference from 0") {
                REQUIRE_THAT(results, Catch::Matchers::Equals(inputs));
            }
        }
        WHEN("resetting in between calculations and halving sample time") {
            std::vector<float> results(8);
            std::vector<float> inputs = {0, 1, 2, 4, 8, 16, 32, 64};
            std::transform(inputs.cbegin(), inputs.cend(), results.begin(),
                           [&p](const float& error) {
                               p.reset();
                               return p.compute(error, 0.5);
                           });
            THEN(
                "the result should always calculate a difference from 0, "
                "divided by the sample time of 0.5") {
                std::vector<float> expected(8);
                std::transform(inputs.cbegin(), inputs.cend(), expected.begin(),
                               [](const float& error) { return error / 0.5; });
                REQUIRE_THAT(results, Catch::Matchers::Equals(expected));
            }
        }
    }

    GIVEN("a PID controller with only ki and no windup limiters") {
        auto p = PID(0, 1.0, 0, 1.0);
        WHEN("repeatedly calculating controls from positive errors") {
            std::vector<float> results(8);
            std::vector<float> inputs = {2, 2, 2, 2, 2, 2, 2, 2};
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the result should accumulate without bound") {
                std::vector<float> intended;
                for (size_t i = 0; i < inputs.size(); i++) {
                    intended.push_back((i + 1) * 2);
                }
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }

        WHEN("repeatedly calculating controls from negative errors") {
            std::vector<float> results(8);
            std::vector<float> inputs = {-10, -10, -10, -10,
                                         -10, -10, -10, -10};
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the result should accumulate without bound") {
                std::vector<float> intended;
                for (size_t i = 0; i < inputs.size(); i++) {
                    intended.push_back((i + 1) * -10.0);
                }
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }

        WHEN("alternating input signs") {
            std::vector<float> results(8);
            std::vector<float> inputs = {10, -10, -10, 10, 10, 10, -10, -10};
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the integral term is properly cancelled") {
                std::vector<float> intended = {10, 0, -10, 0, 10, 20, 10, 0};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
    }

    GIVEN("a PID controller with only ki and a windup limiter") {
        auto p = PID(0, 2, 0, 1.0, 16, -12);
        WHEN("repeatedly calculating controls from positive errors") {
            std::vector<float> inputs = {3, 3, 3, 3, 3, 3, 3, 3};
            std::vector<float> results(8);
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the result should accumulate and clip at bound") {
                std::vector<float> intended = {6, 12, 16, 16, 16, 16, 16, 16};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }

        WHEN("repeatedly calculating controls from negative errors") {
            std::vector<float> inputs = {-2, -2, -2, -2, -2, -2, -2, -2};
            std::vector<float> results(8);
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the result should accumulate and clip at bound") {
                std::vector<float> intended = {-4,  -8,  -12, -12,
                                               -12, -12, -12, -12};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }

        WHEN("alternating input signs") {
            std::vector<float> results(6);
            std::vector<float> inputs = {5, 10, -8, -5, -2, 6};
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN(
                "the integral term is properly cancelled by negating the "
                "input") {
                std::vector<float> intended = {10, 16, 0, -10, -12, 0};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
    }

    GIVEN("a PID controller with all coeffs set and windup limiters present") {
        auto p = PID(2, -1, 1, 1.0, 10, -12);
        WHEN("repeatedly calculating controls") {
            THEN("the result should behave correctly from first principles") {
                REQUIRE(p.compute(1) == 2);
                REQUIRE(p.last_error() == 1);
                REQUIRE(p.last_iterm() == -1);
                REQUIRE(p.compute(2) == 2);
                REQUIRE(p.last_error() == 2);
                REQUIRE(p.last_iterm() == -3);
            }
        }
    }

    GIVEN(
        "a pid controller with an iterm and integrator reset armed with a "
        "positive error value") {
        auto p = PID(0, 1, 0, 1);
        p.arm_integrator_reset(25);
        WHEN("repeatedly calculating controls without an error zero-cross") {
            std::vector<float> inputs = {3, 3, 3, 3, 3, 3, 3, 3};
            std::vector<float> results(8);
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the integrator term should accumulate") {
                std::vector<float> intended = {3, 6, 9, 12, 15, 18, 21, 24};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
        WHEN(
            "repeatedly calculating results that include a single error "
            "zero-cross") {
            std::vector<float> inputs = {3, 3, 3, 3, -3, -3, -3, -3};
            std::vector<float> results(8);
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the integrator should be reset when the error crosses zero") {
                std::vector<float> intended = {3, 6, 9, 12, -3, -6, -9, -12};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
        WHEN(
            "repeatedly calculating results that include multiple error "
            "zero-crosses") {
            std::vector<float> inputs = {3, 3, -3, -3, 1, -1, 2, -2};
            std::vector<float> results(8);
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN(
                "the integrator should be reset only after the first zero "
                "cross") {
                std::vector<float> intended = {3, 6, -3, -6, -5, -6, -4, -6};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
    }

    GIVEN(
        "a pid controller with an iterm and integrator reset armed with a "
        "negative error value") {
        auto p = PID(0, 1, 0, 1);
        p.arm_integrator_reset(-25);
        WHEN("repeatedly calculating controls without an error zero-cross") {
            std::vector<float> inputs = {-3, -3, -3, -3, -3, -3, -3, -3};
            std::vector<float> results(8);
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the integrator term should accumulate") {
                std::vector<float> intended = {-3,  -6,  -9,  -12,
                                               -15, -18, -21, -24};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
        WHEN(
            "repeatedly calculating results that include a single error "
            "zero-cross") {
            std::vector<float> inputs = {-3, -3, -3, -3, 3, 3, 3, 3};
            std::vector<float> results(8);
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN("the integrator should be reset when the error crosses zero") {
                std::vector<float> intended = {-3, -6, -9, -12, 3, 6, 9, 12};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
        WHEN(
            "repeatedly calculating results that include multiple error "
            "zero-crosses") {
            std::vector<float> inputs = {-3, -3, 3, 3, -1, 1, -2, 2};
            std::vector<float> results(8);
            std::transform(
                inputs.cbegin(), inputs.cend(), results.begin(),
                [&p](const float& error) { return p.compute(error); });
            THEN(
                "the integrator should be reset only after the first zero "
                "cross") {
                std::vector<float> intended = {-3, -6, 3, 6, 5, 6, 4, 6};
                REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
            }
        }
    }
}
