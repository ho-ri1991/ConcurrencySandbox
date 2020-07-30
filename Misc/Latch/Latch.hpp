#include <mutex>
#include <condition_variable>

namespace my
{

class Latch
{
private:
  std::mutex mLock;
  std::condition_variable mCond;
  std::size_t mCount;
public:
  Latch(std::size_t count);
  void wait();
};

}

