#pragma once
#include <atomic>
#include <array>
#include <cassert>

class PetersonLock
{
private:
  static std::atomic<int> sIDCount;
  static int getID()
  {
    // it is implementation defined whether the initialization of thread_local variable is deferred or not
    thread_local char id = sIDCount++;
    return id;
  }
  std::array<std::atomic<bool>, 2> mFlags;
  std::atomic<int> mVictim;
public:
  PetersonLock() noexcept:
    mFlags({false, false}),
    mVictim(0)
  {
  }
  PetersonLock(const PetersonLock&) = delete;
  PetersonLock(PetersonLock&&) = delete;
  PetersonLock& operator=(const PetersonLock&) = delete;
  PetersonLock& operator=(PetersonLock&&) = delete;
  ~PetersonLock() = default;
  void lock()
  {
    int i = getID();
    int j = 1 - i;
    assert(i == 0 || i == 1);
    mFlags[i] = true;
    mVictim = i;
    while(mFlags[j] && mVictim == i);
  }
  void unlock()
  {
    int i = getID();
    assert(i == 0 || i == 1);
    mFlags[i] = false;
  }
};

