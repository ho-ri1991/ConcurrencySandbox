#pragma once

#include <condition_variable>
#include <mutex>

namespace my {
class Barrier {
private:
  std::mutex mLock;
  std::condition_variable mCond;
  unsigned int mGeneration;
  unsigned int mCount;
  unsigned int mNum;

public:
  Barrier(unsigned int n);
  void wait();
};
} // namespace my
