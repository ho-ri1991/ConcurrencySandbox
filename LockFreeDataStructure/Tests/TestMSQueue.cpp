#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <random>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include "MSQueue.hpp"

BOOST_AUTO_TEST_CASE(TestMSQueue)
{
  {
    std::promise<void> start;
    auto fut = start.get_future().share();
    std::vector<std::future<void>> ready;
    std::vector<std::future<void>> pushDone;
    std::vector<std::future<std::vector<std::pair<int, int>>>> popDone;
    static constexpr std::size_t numPush = 10000;
    static constexpr std::size_t numPushThread = 8;
    static constexpr std::size_t numPopThread = 8;

    MSQueue<std::pair<int, int>> queue1;
    for(std::size_t i = 0; i < numPushThread; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      pushDone.push_back(std::async(std::launch::async, [&queue1, i, fut, promise = std::move(promise)]()mutable{
        promise.set_value();
        fut.wait();
        for(std::size_t j = 0; j < numPush; ++j)
        {
          queue1.push(std::make_pair(i, j));
        }
      }));
    }
    for(std::size_t i = 0; i < numPopThread; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      popDone.push_back(std::async(std::launch::async, [&queue1, fut, promise = std::move(promise)]() mutable {
        promise.set_value();
        fut.wait();
        std::vector<std::pair<int, int>> ans;
        for(std::size_t i = 0; i < numPush; ++i)
        {
          auto p = queue1.tryPop();
          while(!p)
          {
            p = queue1.tryPop();
          }
          ans.push_back(*p);
        }
        return ans;
      }));
    }
    for(auto& fut: ready)
    {
      fut.wait();
    }
    start.set_value();

    for(auto& fut: popDone)
    {
      std::vector<std::vector<int>> actuals(numPushThread);
      auto vec = fut.get();
      for(auto [i, j]: vec)
      {
        actuals[i].push_back(j);
      }
      for(auto& vec: actuals)
      {
        for(int i = 0; i < static_cast<int>(vec.size()) - 1; ++i)
        {
          BOOST_CHECK_LT(vec[i], vec[i + 1]);
        }
      }
    }
  }
  {
    std::promise<void> start;
    auto fut = start.get_future().share();
    std::vector<std::future<void>> ready;
    std::vector<std::future<void>> pushDone;
    std::vector<std::future<std::vector<int>>> popDone;
    static constexpr std::size_t numPush = 10000;
    static constexpr std::size_t numPushThread = 4;
    static constexpr std::size_t numPopPushThread = 4;
    static constexpr std::size_t numPopThread = 4;

    MSQueue<int> queue1;
    MSQueue<int> queue2;
    for(std::size_t i = 0; i < numPushThread; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      pushDone.push_back(std::async(std::launch::async, [&queue1, i, fut, p = std::move(promise)]()mutable{
        p.set_value();
        fut.wait();
        for(std::size_t j = i * numPush; j < (i + 1) * numPush; ++j)
        {
          queue1.push(j);
        }
      }));
    }
    for(std::size_t i = 0; i < numPopPushThread; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      pushDone.push_back(std::async(std::launch::async, [&queue1, &queue2, fut, p = std::move(promise)]()mutable{
        p.set_value();
        fut.wait();
        for(std::size_t i = 0; i < numPush; ++i)
        {
          auto p = queue1.tryPop();
          while(!p)
          {
            p = queue1.tryPop();
          }
          queue2.push(*p);
        }
      }));
    }
    for(std::size_t i = 0; i < numPopThread; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      popDone.push_back(std::async(std::launch::async, [&queue2, fut, p = std::move(promise)]()mutable{
        p.set_value();
        fut.wait();
        std::vector<int> ans;
        for(std::size_t i = 0; i < numPush; ++i)
        {
          auto p = queue2.tryPop();
          while(!p)
          {
            p = queue2.tryPop();
          }
          ans.push_back(*p);
        }
        return ans;
      }));
    }

    for(auto& fut: ready)
    {
      fut.wait();
    }
    start.set_value();

    for(auto& fut: pushDone)
    {
      fut.wait();
    }
    std::vector<int> actuals;
    for(auto& fut: popDone)
    {
      auto vec = fut.get();
      actuals.insert(actuals.end(), vec.begin(), vec.end());
    }
    std::sort(actuals.begin(), actuals.end());

    std::vector<int> expected;
    for(std::size_t i = 0; i < numPush * numPushThread; ++i)
    {
      expected.push_back(i);
    }
    BOOST_CHECK_EQUAL_COLLECTIONS(actuals.begin(), actuals.end(), expected.begin(), expected.end());
  }
}

