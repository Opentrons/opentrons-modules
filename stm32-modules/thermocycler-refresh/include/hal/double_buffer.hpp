#pragma once
#include <array>

namespace double_buffer {
/*
** Doublebuffering helper.
**
** This maintains two buffers (as std::array), initialized in the class body.
*Its
** main use is with pointers and therefore it is not copyable or movable.
**
** This double-buffer can be used with an interface that requires a pointer to
** a buffer for some probably hardware-assisted communication method, where a
** buffer needs to be committed into the subsystem and not touched until the
** subsystem indicates it is ready. The other buffer meanwhile can be used for
** whatever the application requires.
**
** Swapping should be done under the control of some system that knows that
** each subsystem is ready.
*/
template <typename Datatype, size_t Size>
class DoubleBuffer {
    using Buffer = std::array<Datatype, Size>;

  public:
    DoubleBuffer() : _committed(&_a), _accessible(&_b) {}
    DoubleBuffer(const DoubleBuffer& other) = delete;
    auto operator=(const DoubleBuffer& other) -> DoubleBuffer& = delete;
    DoubleBuffer(DoubleBuffer&& other) noexcept = delete;
    auto operator=(DoubleBuffer&& other) noexcept -> DoubleBuffer& = delete;
    ~DoubleBuffer() = default;

    [[nodiscard]] auto committed() const -> Buffer* { return _committed; }
    [[nodiscard]] auto accessible() const -> Buffer* { return _accessible; }

    void swap() {
        Buffer* temp = _committed;
        _committed = _accessible;
        _accessible = temp;
    }

  private:
    Buffer _a = {};
    Buffer _b = {};

    Buffer* _committed;
    Buffer* _accessible;
};
};  // namespace double_buffer
