#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include "ArrayQueueLock.hpp"
#include "CLHQueueLock.hpp"

BOOST_AUTO_TEST_CASE(TestArrayQueueLock)
{
  static constexpr std::size_t numThread = 16;
  static constexpr std::size_t numIncr = 10000;
  std::size_t count = 0;
  ArrayQueueLock lock;
  {
    std::promise<void> start;
    auto fut = start.get_future().share();
    std::vector<std::future<void>> ready;
    std::vector<std::future<void>> done;
    for(std::size_t i = 0; i < numThread; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      done.push_back(std::async(std::launch::async, [p = std::move(promise), start = fut, &lock, &count]()mutable{
        p.set_value();
        start.wait();
        for(std::size_t i = 0; i < numIncr; ++i)
        {
          std::lock_guard lk(lock);
          count++;
        }
      }));
    }
    for(auto& fut: ready)
    {
      fut.wait();
    }
    start.set_value();
    for(auto& fut: done)
    {
      fut.wait();
    }
  }
  BOOST_CHECK_EQUAL(count, numThread * numIncr);
}

BOOST_AUTO_TEST_CASE(TestCLHQueueLock)
{
  static constexpr std::size_t numThread = 16;
  static constexpr std::size_t numIncr = 10000;
  std::size_t count = 0;
  CLHQueueLock lock;
  {
    std::promise<void> start;
    auto fut = start.get_future().share();
    std::vector<std::future<void>> ready;
    std::vector<std::future<void>> done;
    for(std::size_t i = 0; i < numThread; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      done.push_back(std::async(std::launch::async, [p = std::move(promise), start = fut, &lock, &count]()mutable{
        p.set_value();
        start.wait();
        for(std::size_t i = 0; i < numIncr; ++i)
        {
          std::lock_guard lk(lock);
          count++;
        }
      }));
    }
    for(auto& fut: ready)
    {
      fut.wait();
    }
    start.set_value();
    for(auto& fut: done)
    {
      fut.wait();
    }
  }
  BOOST_CHECK_EQUAL(count, numThread * numIncr);
}

