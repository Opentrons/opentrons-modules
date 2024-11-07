#pragma once

namespace circular_buffer {

template <typename T>
class CircularBuffer {
  public:
    explicit CircularBuffer(std::size_t buffer_size, bool allow_overwrite)
        : _buffer(std::make_unique<T[]>(buffer_size)),
          _max_size(buffer_size),
          _overwrite(allow_overwrite) {}

    [[nodiscard]] auto empty() const -> bool { return _count == 0; }

    [[nodiscard]] auto full() const -> bool { return _count == _max_size; }

    [[nodiscard]] auto capacity() const -> std::size_t { return _max_size; }

    [[nodiscard]] auto size() const -> std::size_t { return _count; }

    auto enqueue(T item) -> bool {
        if (!_overwrite && full()) {
            return false;
        }

        _buffer[_tail] = item;
        _tail = (_tail + 1) % _max_size;

        if (full()) {
            _head = (_head + 1) % _max_size;
        } else {
            _count++;
        }
        return true;
    }

    auto dequeue() -> T {
        T item = _buffer[_head];
        _head = (_head + 1) % _max_size;
        _count--;

        return item;
    }

    auto reset() -> void {
        while (!empty()) {
            static_cast<void>(dequeue());
        }
    }

  private:
    std::unique_ptr<T[]> _buffer;
    const std::size_t _max_size;
    bool _overwrite;
    std::size_t _head = 0;
    std::size_t _tail = 0;
    std::size_t _count = 0;
};

}  // namespace circular_buffer