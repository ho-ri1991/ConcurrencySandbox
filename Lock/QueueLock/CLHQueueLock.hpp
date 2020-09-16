#pragma once

#include <atomic>
#include <memory>

class CLHQueueLock
{
private:
  struct Node
  {
    std::atomic<bool> mLocked;
    Node(bool locked = true): mLocked(locked) {}
  };
  static thread_local std::unique_ptr<Node> sMyNode;
  static thread_local Node* sPredNode;
  std::atomic<Node*> mTail;
public:
  CLHQueueLock();
  ~CLHQueueLock();
  void lock();
  void unlock();
};

