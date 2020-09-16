#include "MCSQueueLock.hpp"
#include <cassert>

thread_local MCSQueueLock::Node MCSQueueLock::sMyNode;

MCSQueueLock::MCSQueueLock(): mTail(nullptr) {};
void MCSQueueLock::lock()
{
  sMyNode.mLocked.store(true, std::memory_order_relaxed);
  sMyNode.mNext.store(nullptr, std::memory_order_relaxed);
  auto pred = mTail.exchange(&sMyNode, std::memory_order_acq_rel);
  if(!pred)
  {
    return;
  }
  pred->mNext.store(&sMyNode, std::memory_order_release);
  while(sMyNode.mLocked.load(std::memory_order_acquire));
}

void MCSQueueLock::unlock()
{
  auto node = &sMyNode;
  if(sMyNode.mNext.load(std::memory_order_acquire) == nullptr)
  {
    if(mTail.compare_exchange_strong(node, nullptr, std::memory_order_release, std::memory_order_relaxed))
    {
      return;
    }
    while(sMyNode.mNext.load(std::memory_order_acquire) == nullptr);
  }
  auto next = sMyNode.mNext.load(std::memory_order_relaxed);
  assert(next->mLocked);
  next->mLocked.store(false, std::memory_order_release);
  sMyNode.mNext.store(nullptr, std::memory_order_relaxed);
}

