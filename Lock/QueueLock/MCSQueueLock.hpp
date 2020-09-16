#pragma once

#include <atomic>
#include <memory>

class MCSQueueLock
{
private:
  struct Node
  {
    std::atomic<Node*> mNext;
    std::atomic<bool> mLocked;
    Node(): mNext(nullptr), mLocked(false) {}
  };
  std::atomic<Node*> mTail;
  static thread_local Node sMyNode;
public:
  MCSQueueLock();
  void lock();
  void unlock();
};

