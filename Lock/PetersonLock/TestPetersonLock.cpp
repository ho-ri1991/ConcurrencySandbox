#include <iostream>
#include <thread>
#include <type_traits>
#include <mutex>
#include "PetersonLock.hpp"

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

int main()
{
  int cnt = 0;
  PetersonLock lock;
  jthread t1([&cnt, &lock](){
    for(int i = 0; i < 1000; ++i)
    {
      std::lock_guard<PetersonLock> l(lock);
      ++cnt;
    }
  });
  jthread t2([&cnt, &lock](){
    for(int i = 0; i < 1000; ++i)
    {
      std::lock_guard<PetersonLock> l(lock);
      ++cnt;
    }
  });
  t1.join();
  t2.join();
  assert(cnt == 2000);
  std::cout << cnt << std::endl;
  
  return 0;
}

