#include "catch2/catch.hpp"
#include "ot_utils/core/message_utils.hpp"

using namespace ot_utils::utils;

struct A {};
struct B {};
struct C {};
struct D {};

TEST_CASE("TupleToVariant works") {
    GIVEN("a tuple of three types") {
        using T = std::tuple<A,B,C>;
        THEN("TupleToVariant gives a variant of those types") {
            using V = TupleToVariant<T>::type;
            REQUIRE(std::variant_size_v<V> == 3);
            auto v1 = V();
            v1 = A();
            REQUIRE(v1.index() == 0);
            v1 = B();
            REQUIRE(v1.index() == 1);
            v1 = C();
            REQUIRE(v1.index() == 2);
        }
    }
}

TEST_CASE("TuplesToVariants works") {
    GIVEN("two tuples of two types each") {
        using T1 = std::tuple<A,B>;
        using T2 = std::tuple<C,D>;
        THEN("TuplesToVariants gives a variant of all four types") {
            using V = TuplesToVariants<T1, T2>::type;
            REQUIRE(std::variant_size_v<V> == 4);
            auto v1 = V();
            v1 = A();
            REQUIRE(v1.index() == 0);
            v1 = B();
            REQUIRE(v1.index() == 1);
            v1 = C();
            REQUIRE(v1.index() == 2);
            v1 = D();
            REQUIRE(v1.index() == 3);
        }
    }
}

TEST_CASE("VariantCat works") {
    GIVEN("a variant with 2 types") {
        using V1 = std::variant<A,B>;
        REQUIRE(std::variant_size_v<V1> == 2);
        WHEN("concatenating them to a monostate") {
            using V2 = VariantCat<std::variant<std::monostate>, V1>::type;
            THEN("the monostate is added") {
                REQUIRE(std::variant_size_v<V2> == 3);
                auto v = V2();
                REQUIRE(v.index() == 0);
                v = A();
                REQUIRE(v.index() == 1);
                v = B();
                REQUIRE(v.index() == 2);
            }
        }
    }
}
