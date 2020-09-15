#pragma once

#include <atomic>
#include <new>

namespace my
{
inline constexpr std::size_t hardware_destructive_interference_size = 64;
}

class ArrayQueueLock
{
private:
  struct Node
  {
    alignas(my::hardware_destructive_interference_size) std::atomic<bool> mFlag;
    Node(): mFlag(false) {}
  };
  static constexpr unsigned int sCapacity = 64;
  static thread_local unsigned int sIndex;
  Node mArray[sCapacity];
  std::atomic<unsigned int> mTail;
public:
  ArrayQueueLock();
  void lock();
  void unlock();
};

