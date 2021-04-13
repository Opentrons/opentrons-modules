/*
** The ack cache is a way to keep around gcode messages that have not yet
** been executed. It maps a copy of the actual gcode object sent by the
** host with the internal message id.
**
** This is not done with an actual map because that would need to allocate.
*/

#pragma once

#include <algorithm>
#include <array>
#include <variant>

template <size_t max_size, typename... Contents>
struct AckCache {
    using Payload = std::variant<std::monostate, Contents...>;
    AckCache() : cache{CacheWrapper{0, Payload(std::monostate())}} {}

    static constexpr size_t size = max_size;

    template <typename ContentElement>
    auto add(const ContentElement& element) -> uint32_t {
        for (auto cache_element = cache.begin(); cache_element != cache.end();
             cache_element++) {
            if (std::holds_alternative<std::monostate>(
                    cache_element->contents)) {
                cache_element->contents = Payload(element);
                cache_element->id = next_id;
                next_id++;
                if (next_id == 0) {
                    next_id++;
                }
                return cache_element->id;
            }
        }
        return 0;
    }

    auto remove_if_present(uint32_t id) -> Payload {
        auto which = std::find_if(
            cache.begin(), cache.end(),
            [&id](auto element) -> bool { return element.id == id; });
        if (which == cache.end()) {
            return Payload(std::monostate());
        }
        auto payload = which->contents;
        which->contents = std::monostate();
        which->id = 0;
        return payload;
    }

    auto clear() -> void {
        for (auto& cache_element : cache) {
            cache_element.contents = std::monostate();
            cache_element.id = 0;
        }
    }

  private:
    // Present only for testing; do not use
    friend class _AckCacheTestHook;
    struct CacheWrapper {
        uint32_t id;
        Payload contents;
    };
    std::array<CacheWrapper, max_size> cache;
    uint32_t next_id = 1;
};
