#include "Latch.hpp"

namespace my
{

Latch::Latch(std::size_t count): mCount(count) {}

void Latch::wait()
{
  std::unique_lock lk(mLock);
  --mCount;
  if(mCount)
  {
    mCond.wait(lk, [this](){ return mCount == 0; });
  }
  else
  {
    mCond.notify_all();
  }
}

}

