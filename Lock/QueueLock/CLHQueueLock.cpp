#include "CLHQueueLock.hpp"

thread_local std::unique_ptr<CLHQueueLock::Node> CLHQueueLock::sMyNode = std::make_unique<CLHQueueLock::Node>();
thread_local CLHQueueLock::Node* CLHQueueLock::sPredNode = nullptr;

CLHQueueLock::CLHQueueLock(): mTail(new Node(false))
{
}
CLHQueueLock::~CLHQueueLock()
{
  auto tail = mTail.exchange(nullptr);
  if(tail)
  {
    delete tail;
  }
}
void CLHQueueLock::lock()
{
  sMyNode->mLocked.store(true, std::memory_order_relaxed);
  sPredNode = mTail.exchange(sMyNode.get(), std::memory_order_acq_rel);
  while(sPredNode->mLocked.load(std::memory_order_acquire));
}
void CLHQueueLock::unlock()
{
  auto oldMyNode = sMyNode.release();
  sMyNode.reset(sPredNode);
  oldMyNode->mLocked.store(false, std::memory_order_release);
}

