#pragma once

#include <mutex>
#include <condition_variable>
#include <cassert>

class SharedLock
{
private:
  std::mutex mLock;
  std::condition_variable mCond;
  std::size_t mReader;
  bool mWriter;
public:
  SharedLock(): mReader(0), mWriter(false) {}
  void lock()
  {
    std::unique_lock lk(mLock);
    while(mWriter)
    {
      mCond.wait(lk, [this]{ return !mWriter; });
    }
    mWriter = true;
    while(mReader)
    {
      mCond.wait(lk, [this]{ return mReader == 0; });
    }
  }
  void unlock()
  {
    std::lock_guard lk(mLock);
    assert(mReader == 0);
    mWriter = false;
    mCond.notify_all();
  }
  void lock_shared()
  {
    std::unique_lock lk(mLock);
    while(mWriter)
    {
      mCond.wait(lk, [this]{ return !mWriter; });
    }
    ++mReader;
  }
  void unlock_shared()
  {
    std::lock_guard lk(mLock);
    --mReader;
    assert(0 <= mReader);
    mCond.notify_all();
  }
};

