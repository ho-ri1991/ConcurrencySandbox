#include "CLHTimeOutQueueLock.hpp"

CLHTimeOutQueueLock::Node CLHTimeOutQueueLock::sAvaiable;
thread_local CLHTimeOutQueueLock::Node* CLHTimeOutQueueLock::sMyNode = nullptr;

CLHTimeOutQueueLock::CLHTimeOutQueueLock(): mTail(nullptr) {}
CLHTimeOutQueueLock::~CLHTimeOutQueueLock()
{
  auto node = mTail.exchange(nullptr);
  while(node && node != &sAvaiable)
  {
    auto pred = node->mPred.load();
    delete node;
    node = pred;
  }
}
void CLHTimeOutQueueLock::unlock()
{
  Node* node = sMyNode;
  if(!mTail.compare_exchange_strong(node, nullptr))
  {
    sMyNode->mPred.store(&sAvaiable);
  }
  else
  {
    delete sMyNode;
  }
}

