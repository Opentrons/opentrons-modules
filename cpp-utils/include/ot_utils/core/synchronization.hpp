#pragma once

namespace ot_utils {
namespace synchronization {

template <typename S>
concept LockableProtocol = requires(S& s) {
    {s.acquire()};
    {s.release()};
};

template <LockableProtocol Lockable>
requires(!std::movable<Lockable> && !std::copyable<Lockable>) class Lock {
  public:
    Lock(Lockable& s) : lockable(s) { lockable.acquire(); }
    Lock(const Lock&) = delete;
    Lock(Lock&&) = delete;
    auto operator=(const Lock&) -> Lock& = delete;
    auto operator=(Lock&&) -> Lock&& = delete;
    ~Lock() { lockable.release(); }

  private:
    Lockable& lockable;
};

}  // namespace synchronization
}  // namespace ot_utils
