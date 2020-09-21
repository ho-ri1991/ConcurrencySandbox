#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <unordered_set>
#include <limits>
#include "ArrayQueueLock.hpp"
#include "CLHQueueLock.hpp"
#include "MCSQueueLock.hpp"
#include "CLHTimeOutQueueLock.hpp"
#include "AtomicStampedPointer.hpp"
#include "CompositeQueueLock.hpp"

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

BOOST_AUTO_TEST_CASE(TestMCSQueueLock)
{
  static constexpr std::size_t numThread = 16;
  static constexpr std::size_t numIncr = 10000;
  std::size_t count = 0;
  MCSQueueLock lock;
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

BOOST_AUTO_TEST_CASE(TestCLHTimeOutQueueLock)
{
  using namespace std::literals::chrono_literals;
  CLHTimeOutQueueLock lock;
  static constexpr std::size_t numThread = 16;
  static constexpr std::size_t numIncr = 100;
  std::size_t count = 0;
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
          while(!lock.tryLock(100us));
          std::this_thread::sleep_for(50us);
          count++;
          lock.unlock();
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

BOOST_AUTO_TEST_CASE(TestAtomicStampedPointerSingleThread)
{
  using StampedPointer = AtomicStampedPointer<int>;
  int num = 42;
  StampedPointer::StampType stamp = 0;
  AtomicStampedPointer<int> stampedPointer;
  stampedPointer.store(&num, stamp);
  {
    auto [p, i] = stampedPointer.load();
    BOOST_CHECK_EQUAL(p, &num);
    BOOST_CHECK_EQUAL(i, stamp);
  }
  int num1 = 42;
  StampedPointer::StampType stamp1 = 1;
  {
    auto [p, i] = stampedPointer.exchange(&num1, stamp1);
    BOOST_CHECK_EQUAL(p, &num);
    BOOST_CHECK_EQUAL(i, stamp);
    auto [p1, i1] = stampedPointer.load();
    BOOST_CHECK_EQUAL(p1, &num1);
    BOOST_CHECK_EQUAL(i1, stamp1);
  }
  int num2 = 42;
  StampedPointer::StampType stamp2 = 2;
  {
    auto pointer = &num;
    StampedPointer::StampType stamp = 0;
    auto b = stampedPointer.compare_exchange_strong(pointer, &num2, stamp, stamp2);
    BOOST_CHECK(!b);
    BOOST_CHECK_EQUAL(pointer, &num1);
    BOOST_CHECK_EQUAL(stamp, stamp1);
    b = stampedPointer.compare_exchange_strong(pointer, &num2, stamp, stamp2);
    BOOST_CHECK(b);
    auto [p, i] = stampedPointer.load();
    BOOST_CHECK_EQUAL(p, &num2);
    BOOST_CHECK_EQUAL(i, stamp2);
  }
  {
    int* pointer = reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(&num) | 0xFFFF800000000000);
    stampedPointer.store(pointer, 0);
    auto [p, i] = stampedPointer.load();
    BOOST_CHECK_EQUAL(pointer, p);
    BOOST_CHECK_EQUAL(i, 0);
  }
}

BOOST_AUTO_TEST_CASE(TestAtomicStampedPointer)
{
  static constexpr std::size_t numThread = 16;
  static constexpr std::size_t numIncr = 100;
  std::vector<int> arr(numThread);
  std::unordered_set<int*> set;
  for(std::size_t i = 0; i < arr.size(); ++i)
  {
    set.insert(&arr[i]);
  }
  AtomicStampedPointer<int> stampedPointer;
  stampedPointer.store(&arr[0], 0, std::memory_order_relaxed);
  std::promise<void> start;
  auto fut = start.get_future().share();
  std::vector<std::future<void>> ready;
  std::vector<std::future<void>> done;
  for(std::size_t i = 0; i < numThread; ++i)
  {
    std::promise<void> promise;
    ready.push_back(promise.get_future());
    done.push_back(std::async(std::launch::async, [p = std::move(promise), start = fut, myPointer = &arr[i], &stampedPointer, &set]()mutable{
      p.set_value();
      start.wait();
      for(std::size_t i = 0; i < numIncr; ++i)
      {
        auto [p, s] = stampedPointer.load(std::memory_order_relaxed);
        while(!stampedPointer.compare_exchange_strong(p, myPointer, s, s + 1, std::memory_order_relaxed));
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
  auto [p, s] = stampedPointer.load(std::memory_order_relaxed);
  BOOST_CHECK_EQUAL(set.count(p), 1);
  BOOST_CHECK_EQUAL(s, numThread * numIncr);
}

BOOST_AUTO_TEST_CASE(TestCompositeQueueLock)
{
  using namespace std::literals::chrono_literals;
  CompositeQueueLock lock;
  static constexpr std::size_t numThread = 16;
  static constexpr std::size_t numIncr = 100;
  std::size_t count = 0;
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
          while(!lock.tryLock(1ms));
          std::this_thread::sleep_for(50us);
          count++;
          lock.unlock();
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

