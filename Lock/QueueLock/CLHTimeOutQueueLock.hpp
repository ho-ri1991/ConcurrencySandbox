#pragma once

#include <atomic>
#include <chrono>

class CLHTimeOutQueueLock
{
private:
  struct Node
  {
    std::atomic<Node*> mPred;
    Node(): mPred(nullptr) {}
  };
  static Node sAvaiable;
  static thread_local Node* sMyNode;
  std::atomic<Node*> mTail;
public:
  CLHTimeOutQueueLock();
  ~CLHTimeOutQueueLock();
  template <typename Rep, typename Period>
  bool tryLock(const std::chrono::duration<Rep, Period>& relTime);
  void unlock();
};

#include "CLHTimeOutQueueLock.inl"
