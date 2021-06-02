#include <limits>
#include <variant>

#include "catch2/catch.hpp"
#include "heater-shaker/ack_cache.hpp"

struct Element1 {
    uint32_t foo;
};

struct Element2 {
    float bar;
};

class _AckCacheTestHook {
  public:
    template <typename CacheKind>
    static void set_id(CacheKind& cache, uint32_t next_id) {
        cache.next_id = next_id;
    }
};

SCENARIO("testing ack cache functionality") {
    GIVEN("an ack cache with a couple elements") {
        auto cache = AckCache<8, Element1, Element2>();
        WHEN("finding a value in an empty cache") {
            auto found = cache.remove_if_present(1231254);
            THEN("nothing should be found") {
                REQUIRE(std::holds_alternative<std::monostate>(found));
            }
        }
        WHEN("checking empty() in an empty cache") { REQUIRE(cache.empty()); }
        WHEN("adding to the cache") {
            auto id1 = cache.add(Element1(10));
            auto id2 = cache.add(Element2(2.5));
            THEN("you should get a different id every time") {
                REQUIRE(id1 != id2);
                REQUIRE(id1 != 0);
            }
            THEN("the cache should not be empty") { REQUIRE(!cache.empty()); }
        }

        WHEN("removing valid items from the cache") {
            auto added1 = Element2(3.4);
            auto added2 = Element2(5.2);
            auto id1 = cache.add(added1);
            auto id2 = cache.add(added2);
            auto removed1 = cache.remove_if_present(id1);
            auto removed2 = cache.remove_if_present(id2);
            THEN("each removed item should be correct") {
                REQUIRE(std::holds_alternative<Element2>(removed1));
                REQUIRE(std::holds_alternative<Element2>(removed2));
                REQUIRE(std::get<Element2>(removed1).bar == added1.bar);
                REQUIRE(std::get<Element2>(removed2).bar == added2.bar);
            }
        }

        WHEN("removing invalid items from a non-empty cache") {
            auto added1 = Element1(2);
            auto added2 = Element2(3.56);
            auto id1 = cache.add(added1);
            auto id2 = cache.add(added2);
            uint32_t bad_id = 99;
            CHECK(bad_id != id1);
            CHECK(bad_id != id2);
            auto removed_invalid = cache.remove_if_present(bad_id);
            THEN("the removed item should be invalid") {
                REQUIRE(
                    std::holds_alternative<std::monostate>(removed_invalid));
            }
            AND_WHEN("subsequently removing a valid item") {
                auto removed2 = cache.remove_if_present(id2);
                THEN("the removed item should be correct") {
                    REQUIRE(std::holds_alternative<Element2>(removed2));
                    REQUIRE(std::get<Element2>(removed2).bar == added2.bar);
                }
            }
        }

        WHEN("forcing the id to roll over") {
            _AckCacheTestHook::set_id(cache,
                                      std::numeric_limits<uint32_t>::max());
            auto added1 = Element1(10);
            auto added2 = Element2(10.22);
            auto id1 = cache.add(added1);
            auto id2 = cache.add(added2);

            THEN("the ids should skip 0") {
                REQUIRE(id1 != 0);
                REQUIRE(id2 != 0);
                REQUIRE(id2 < id1);
            }
            AND_WHEN("removing the items") {
                auto removed2 = cache.remove_if_present(id2);
                auto removed1 = cache.remove_if_present(id1);
                THEN("the removed items should be valid") {
                    REQUIRE(std::holds_alternative<Element2>(removed2));
                    REQUIRE(std::holds_alternative<Element1>(removed1));
                    REQUIRE(std::get<Element2>(removed2).bar == added2.bar);
                    REQUIRE(std::get<Element1>(removed1).foo == added1.foo);
                }
            }
        }
        WHEN("adding another element to a full cache") {
            for (size_t i = 0; i < cache.size; i++) {
                auto throwaway = Element1(i + 2);
                cache.add(throwaway);
            }
            auto rejected = Element2(2.15123);
            auto rejected_id = cache.add(rejected);
            THEN("the element should have id 0") { REQUIRE(rejected_id == 0); }
            AND_WHEN("trying to find the rejected element") {
                auto rejected_removed = cache.remove_if_present(rejected_id);
                THEN("it should not be found") {
                    REQUIRE(std::holds_alternative<std::monostate>(
                        rejected_removed));
                }
            }
        }
    }
}
