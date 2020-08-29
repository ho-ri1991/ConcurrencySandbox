#include <chrono>

template <typename F>
InterruptibleThread::InterruptibleThread(F&& f)
{
  auto task = [this, f = std::move(f)](){
    mInterruptFlag.store(&sInterruptFlag);
    try
    {
      f();
    }
    catch(InterruptException& ex)
    {
    }
  };
  mThread = std::thread(std::move(task));
}
template <typename Pred>
void interruptibleWait(std::unique_lock<std::mutex>& lock, std::condition_variable& cond, Pred pred)
{
  interruptionPoint();
  InterruptibleThread::sInterruptFlag.setConditionVariable(cond);
  interruptionPoint();
  while(!InterruptibleThread::sInterruptFlag.isSet() && !pred())
  {
    cond.wait_for(lock, std::chrono::milliseconds(1));
  }
  interruptionPoint();
}

