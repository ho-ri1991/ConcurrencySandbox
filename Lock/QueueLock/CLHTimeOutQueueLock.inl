template <typename Rep, typename Period>
bool CLHTimeOutQueueLock::tryLock(const std::chrono::duration<Rep, Period>& relTime)
{
  sMyNode = new Node();
  auto pred = mTail.exchange(sMyNode);
  if(pred == nullptr || pred->mPred.load() == &sAvaiable)
  {
    if(pred)
    {
      delete pred;
    }
    return true;
  }
  auto startTime = std::chrono::steady_clock::now();
  auto deadline = startTime + relTime;
  do 
  {
    auto predPred = pred->mPred.load();
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
  if(!mTail.compare_exchange_strong(node, pred))
  {
    sMyNode->mPred.store(pred);
  }
  else
  {
    delete sMyNode;
  }
  return false;
}

