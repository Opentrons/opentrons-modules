#pragma once

namespace circular_buffer {

template <typename T, std::size_t buffer_size> class CircularBuffer {
  public:
    explicit CircularBuffer(bool allow_overwrite = false)
        : _buffer(std::array<T, buffer_size>()), _overwrite(allow_overwrite) {}

    [[nodiscard]] auto empty() const -> bool { return _count == 0; }

    [[nodiscard]] auto full() const -> bool { return _count == buffer_size; }

    [[nodiscard]] auto capacity() const -> std::size_t { return buffer_size; }

    [[nodiscard]] auto size() const -> std::size_t { return _count; }

    auto enqueue(T item) -> bool {
        if (!_overwrite && full()) {
            return false;
        }

        _buffer.at(_tail) = item;
        _tail = (_tail + 1) % buffer_size;

        if (full()) {
            _head = (_head + 1) % buffer_size;
        } else {
            _count++;
        }
        return true;
    }

    auto dequeue() -> T {
        T item = _buffer.at(_head);
        _head = (_head + 1) % buffer_size;
        _count--;

        return item;
    }

    auto reset() -> void {
        while (!empty()) {
            static_cast<void>(dequeue());
        }
    }

  private:
    std::array<T, buffer_size> _buffer;
    bool _overwrite;
    std::size_t _head = 0;
    std::size_t _tail = 0;
    std::size_t _count = 0;
};

} // namespace circular_buffer