#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

class jthread
{
private:
  std::thread t;
public:
  template <typename F, typename... Args>
  jthread(F&& f, Args&&... args): t(std::forward<F>(f), std::forward<Args>(args)...)
  {
    static_assert(std::is_invocable_v<F, Args&&...>);
  }
  jthread(const jthread&) = delete;
  jthread(jthread&&) = default;
  jthread& operator=(const jthread) = delete;
  jthread& operator=(jthread&&) = default;
  void join()
  {
    t.join();
  }
  ~jthread() noexcept
  {
    if(t.joinable())
    {
      t.join();
    }
  }
};

// we can use std::once_flag and std::call_once (or a static variable in a function) instead of double check initialization
class SomeData
{
public:
  void process() {}
};
std::atomic<bool> gInitFlag(false);
std::unique_ptr<SomeData> gData;
std::mutex gLock;
void doSomething()
{
  if(!gInitFlag.load(std::memory_order_acquire))
  {
    std::lock_guard lk(gLock);
    if(!gInitFlag.load(std::memory_order_relaxed)) // we can use a relaxed operation because mutex operation act as a barrier
    {
      gData = std::make_unique<SomeData>();
      gInitFlag.store(true, std::memory_order_release);
    }
  }
  gData->process();
}

int main()
{
  {
    std::vector<jthread> threads;
    threads.reserve(16);
    for(std::size_t i = 0; i < 16; ++i)
    {
      threads.emplace_back([](){ doSomething(); });
    }
  }
  return 0;
}

