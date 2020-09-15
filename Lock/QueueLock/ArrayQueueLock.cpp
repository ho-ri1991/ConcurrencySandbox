#include "ArrayQueueLock.hpp"

thread_local unsigned int ArrayQueueLock::sIndex;

ArrayQueueLock::ArrayQueueLock()
{
  mArray[0].mFlag.store(true);
  mTail.store(0);
}

void ArrayQueueLock::lock()
{
  sIndex = mTail.fetch_add(1, std::memory_order_acq_rel) % sCapacity;
  while(!mArray[sIndex].mFlag.load(std::memory_order_acquire));
}

void ArrayQueueLock::unlock()
{
  mArray[sIndex].mFlag.store(false, std::memory_order_relaxed); // TODO: prove if memory_order_relaxed is acceptable or not in case where the number of thread is strictry smaller than sCapacity
  mArray[(sIndex + 1) % sCapacity].mFlag.store(true, std::memory_order_release);
}

