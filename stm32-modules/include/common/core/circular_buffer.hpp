#pragma once

namespace circular_buffer {

template <typename T, std::size_t MaxSize>
class CircularBuffer {
  public:
    using BackingStore = std::array<T, MaxSize>;
    explicit CircularBuffer(bool allow_overwrite = false)
        : _buffer(std::make_unique<BackingStore>()), _overwrite(allow_overwrite) {}

    [[nodiscard]] auto empty() const -> bool { return _count == 0; }

    [[nodiscard]] auto full() const -> bool { return _count == MaxSize; }

    [[nodiscard]] auto capacity() const -> std::size_t { return MaxSize; }

    [[nodiscard]] auto size() const -> std::size_t { return _count; }

    auto enqueue(T item) -> bool {
        if (!_overwrite && full()) {
            return false;
        }

        (*_buffer)[_tail] = item;
        _tail = (_tail + 1) % MaxSize;

        if (full() && _overwrite) {
            // overwrite the oldest item
            _head = (_head + 1) % MaxSize;
        } else if (!full()) {
            _count++;
        }
        return true;
    }

    auto dequeue(T& item) -> bool {
        if (empty()) {
            return false;
        }
        item = (*_buffer)[_head];
        _head = (_head + 1) % MaxSize;
        _count--;

        return true;
    }

    auto reset() -> void {
        _head = 0;
        _tail = 0;
        _count = 0;
    }

  private:
    std::unique_ptr<BackingStore> _buffer;
    bool _overwrite;
    std::size_t _head = 0;
    std::size_t _tail = 0;
    std::size_t _count = 0;
};

}  // namespace circular_buffer