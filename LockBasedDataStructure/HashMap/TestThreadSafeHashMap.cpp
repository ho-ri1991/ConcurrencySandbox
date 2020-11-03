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
#include "StripedThreadSafeHashMap.hpp"

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

BOOST_AUTO_TEST_CASE(TestStripedThreadSafeHashmap)
{
  StripedThreadSafeHashmap<int, int> map;
  std::promise<void> start;
  auto startFut = start.get_future().share();
  std::vector<std::future<void>> ready;
  std::vector<std::future<void>> done;
  static constexpr std::size_t numModifyThreads = 8;
  static constexpr std::size_t numFindThreads = 8;
  static constexpr std::size_t numInsertPerThread = 4000;
  static constexpr std::size_t removeRatio = 4;
  std::vector<int> keys(numModifyThreads * numInsertPerThread);
  for(std::size_t i = 0; i < numModifyThreads; ++i)
  {
    std::promise<void> p;
    ready.push_back(p.get_future());
    done.push_back(std::async(std::launch::async, [i, &map, p = std::move(p), startFut]()mutable{
      std::size_t start = i * numInsertPerThread;
      std::size_t end = (i + 1) * numInsertPerThread;
      p.set_value();
      startFut.wait();
      for(std::size_t j = start; j < end; ++j)
      {
        map.addOrUpdate(j, j);
      }
      for(std::size_t j = start; j < end; j += removeRatio)
      {
        map.remove(j);
      }
    }));
  }
  for(std::size_t i = 0; i < numFindThreads; ++i)
  {
    std::promise<void> p;
    ready.push_back(p.get_future());
    done.push_back(std::async(std::launch::async, [p = std::move(p), startFut, &map]()mutable{
      p.set_value();
      startFut.wait();
      for(std::size_t i = 0; i < numModifyThreads; ++i)
      {
        std::size_t start = i * numInsertPerThread;
        std::size_t end = (i + 1) * numInsertPerThread;
        for(std::size_t j = start; j < end; ++j)
        {
          if((j - start) % removeRatio == 0)
          {
            continue;
          }
          while(!map.find(j));
        }
      }
    }));
  }
  start.set_value();
  for(auto& fut: done)
  {
    fut.wait();
  }
  for(std::size_t i = 0; i < numModifyThreads; ++i)
  {
    std::size_t start = i * numInsertPerThread;
    std::size_t end = (i + 1) * numInsertPerThread;
    for(std::size_t j = start; j < end; ++j)
    {
      auto res = map.find(j);
      if((j - start) % removeRatio == 0)
      {
        BOOST_CHECK(!res);
      }
      else
      {
        BOOST_CHECK(res);
        BOOST_CHECK_EQUAL(*res, j);
      }
    }
  }
}

