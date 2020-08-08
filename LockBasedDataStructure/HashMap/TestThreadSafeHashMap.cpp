#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <random>
#include "FixedSizeThreadSafeHashMap.hpp"

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

BOOST_AUTO_TEST_CASE(TestFixedSizeThreadSafeHashMap)
{
  FixedSizeThreadSafeHashMap<int, int> map;
  jthread add1([&map]{
    for(int i = 0; i < 100; ++i)
    {
      map.addOrUpdate(i, i);
    }
  });
  jthread add2([&map]{
    for(int i = 100; i < 200; ++i)
    {
      map.addOrUpdate(i, i);
    }
  });
  jthread find1([&map]{
    for(int i = 0; i < 200; ++i)
    {
      if(!map.find(i))
      {
        continue;
      }
    }
  });
  jthread find2([&map]{
    for(int i = 0; i < 200; ++i)
    {
      if(!map.find(i))
      {
        continue;
      }
    }
  });
}

