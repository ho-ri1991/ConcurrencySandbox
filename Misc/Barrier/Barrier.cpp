#include "Barrier.hpp"
#include <ctime>
#include <mutex>

namespace my {
Barrier::Barrier(unsigned int n) : mGeneration(0), mCount(n), mNum(n) {}
void Barrier::wait() {
  std::unique_lock<std::mutex> l(mLock);
  unsigned int curGen = mGeneration;
  --mCount;
  if (mCount == 0) {
    ++mGeneration;
    mCount = mNum;
    mCond.notify_all();
  } else {
    mCond.wait(l, [this, curGen] { return curGen != mGeneration; });
  }
}
} // namespace my
