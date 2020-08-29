#include "InterruptibleThread.hpp"

InterruptFlag::InterruptFlag()
  : mFlag(false)
  , mCond(nullptr) {}
void InterruptFlag::set()
{
  mFlag.store(true);
  std::lock_guard lk(mSetClearCondLock);
  if(mCond)
  {
    mCond->notify_all();
  }
}
bool InterruptFlag::isSet() const
{
  return mFlag.load();
}
void InterruptFlag::setConditionVariable(std::condition_variable& cond)
{
  std::lock_guard lk(mSetClearCondLock);
  mCond = &cond;
}
void InterruptFlag::clearConditionVariable()
{
  std::lock_guard lk(mSetClearCondLock);
  mCond = nullptr;
}

const char* InterruptException::what() const noexcept
{
  return "interruption exception";
}

void InterruptibleThread::join()
{
  mThread.join();
}
void InterruptibleThread::detach()
{
  mThread.detach();
}
bool InterruptibleThread::joinable() const
{
  return mThread.joinable();
}
void InterruptibleThread::interrupt()
{
  mInterruptFlag.load()->set();
}
thread_local InterruptFlag InterruptibleThread::sInterruptFlag;

void interruptionPoint()
{
  if(InterruptibleThread::sInterruptFlag.isSet())
  {
    throw InterruptException();
  }
}
void interruptibleWait(std::unique_lock<std::mutex>& lock, std::condition_variable& cond)
{
  interruptionPoint();
  InterruptibleThread::sInterruptFlag.setConditionVariable(cond);
  interruptionPoint();
  while(!InterruptibleThread::sInterruptFlag.isSet())
  {
    cond.wait_for(lock, std::chrono::milliseconds(1));
  }
  interruptionPoint();
}

