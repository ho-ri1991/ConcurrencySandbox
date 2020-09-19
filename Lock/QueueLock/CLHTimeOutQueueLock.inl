template <typename Rep, typename Period>
bool CLHTimeOutQueueLock::tryLock(const std::chrono::duration<Rep, Period>& relTime)
{
  sMyNode = new Node();
  sMyNode->mPred.store(nullptr, std::memory_order_relaxed);
  auto pred = mTail.exchange(sMyNode, std::memory_order_acq_rel);
  if(pred == nullptr)
  {
    return true;
  }
  else if(pred->mPred.load(std::memory_order_acquire) == &sAvaiable)
  {
    delete pred;
    return true;
  }
  auto startTime = std::chrono::steady_clock::now();
  auto deadline = startTime + relTime;
  do 
  {
    auto predPred = pred->mPred.load(std::memory_order_acquire);
    if(predPred == &sAvaiable)
    {
      delete pred;
      return true;
    }
    else if(predPred != nullptr)
    {
      delete pred;
      pred = predPred;
    }
  }
  while(std::chrono::steady_clock::now() < deadline);
  Node* node = sMyNode;
  if(!mTail.compare_exchange_strong(node, pred, std::memory_order_acq_rel, std::memory_order_relaxed))
  {
    sMyNode->mPred.store(pred, std::memory_order_release);
  }
  else
  {
    delete sMyNode;
  }
  return false;
}

