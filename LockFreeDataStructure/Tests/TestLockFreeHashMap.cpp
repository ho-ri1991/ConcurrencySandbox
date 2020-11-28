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
#include "LockFreeHashMap.hpp"

BOOST_AUTO_TEST_CASE(TestAtomicMarkablePointer)
{
  {
    AtomicMarkablePointer<int> p;
    p.store(nullptr, false);
    {
      auto [q, mark] = p.load();
      BOOST_CHECK(!mark);
      BOOST_CHECK_EQUAL(q, nullptr);
    }
    {
      int x;
      p.store(&x, true);
      auto [q, mark] = p.load();
      BOOST_CHECK(mark);
      BOOST_CHECK_EQUAL(q, &x);
      int* expectedPointer = &x;
      bool expectedMark = true;
      bool success = p.compare_exchange_strong(expectedPointer, nullptr, expectedMark, false);
      BOOST_CHECK(success);
      auto [q1, mark1] = p.load();
      BOOST_CHECK(!mark1);
      BOOST_CHECK_EQUAL(q1, nullptr);
      success = p.compare_exchange_strong(expectedPointer, nullptr, expectedMark, false);
      BOOST_CHECK(!success);
      BOOST_CHECK_EQUAL(expectedPointer, nullptr);
      BOOST_CHECK(!expectedMark);
    }
  }
}
BOOST_AUTO_TEST_CASE(TestSingleThreadLockFreeExtendibleBucket)
{
  static constexpr int sInitialValue = 42;
  struct Initializer
  {
    void operator()(int& val) { val = sInitialValue; }
  };
  static constexpr std::size_t baseArraySize = 1 << 2;
  LockFreeExtendibleBucket<int, baseArraySize, Initializer> bucket;
  BOOST_CHECK_EQUAL(bucket.size(), baseArraySize);
  for(std::size_t i = 0; i < bucket.size(); ++i)
  {
    BOOST_CHECK_EQUAL(bucket[i], sInitialValue);
    bucket[i] = i;
  }
  bucket.extend();
  BOOST_CHECK_EQUAL(bucket.size(), baseArraySize * baseArraySize);
  for(std::size_t i = baseArraySize; i < bucket.size(); ++i)
  {
    BOOST_CHECK_EQUAL(bucket[i], sInitialValue);
    bucket[i] = i;
  }
  bucket.extend();
  BOOST_CHECK_EQUAL(bucket.size(), baseArraySize * baseArraySize * baseArraySize);
  for(std::size_t i = baseArraySize * baseArraySize; i < bucket.size(); ++i)
  {
    BOOST_CHECK_EQUAL(bucket[i], sInitialValue);
    bucket[i] = i;
  }
  for(std::size_t i = 0; i < bucket.size(); ++i)
  {
    BOOST_CHECK_EQUAL(i, i);
  }
}
BOOST_AUTO_TEST_CASE(TestSingleThreadLockFreeHashMap)
{
  {
    LockFreeHashMap<int, int> map;
    BOOST_CHECK(map.insert({42, 42}));
    auto res = map.find(42);
    BOOST_CHECK(res.has_value());
    BOOST_CHECK_EQUAL(*res, 42);
    BOOST_CHECK(!map.insert({42, 42}));
    BOOST_CHECK(map.remove(42));
    BOOST_CHECK(!map.remove(42));

    BOOST_CHECK(map.insert({42, 42}));
    res = map.find(42);
    BOOST_REQUIRE(res.has_value());
    BOOST_CHECK_EQUAL(*res, 42);
    BOOST_CHECK(!map.insert({42, 42}));
    BOOST_CHECK(map.remove(42));
    BOOST_CHECK(!map.remove(42));

    for(std::size_t i = 1; i < 1000; ++i)
    {
      BOOST_CHECK(map.insert({i, -i}));
    }
    for(std::size_t i = 1; i < 1000; ++i)
    {
      auto res = map.find(i);
      BOOST_REQUIRE(res.has_value());
      BOOST_CHECK_EQUAL(*res, -i);
      res = map.find(-i);
      BOOST_CHECK(!res.has_value());
    }
    for(std::size_t i = 1; i < 100; ++i)
    {
      BOOST_CHECK(map.remove(i));
    }
    for(std::size_t i = 1; i < 100; ++i)
    {
      BOOST_CHECK(!map.remove(i));
    }
    for(std::size_t i = 1; i < 1000; ++i)
    {
      auto res = map.find(i);
      if(i < 100)
      {
        BOOST_CHECK(!res.has_value());
      }
      else
      {
        BOOST_REQUIRE(res.has_value());
        BOOST_CHECK_EQUAL(*res, -i);
      }
    }
    for(std::size_t i = 1; i < 1000; ++i)
    {
      auto res = map.insert({i, -i});
      if(i < 100)
      {
        BOOST_CHECK(res);
      }
      else
      {
        BOOST_CHECK(!res);
      }
    }
    for(std::size_t i = 1; i < 1000; ++i)
    {
      auto res = map.find(i);
      BOOST_REQUIRE(res.has_value());
      BOOST_CHECK_EQUAL(*res, -i);
    }
  }
}
BOOST_AUTO_TEST_CASE(TestLockFreeHashMap)
{
  {
    LockFreeHashMap<int, int> map;
    std::promise<void> start;
    std::vector<std::future<void>> ready;
    std::vector<std::future<void>> done;
    static constexpr std::size_t numInsertThreads = 8;
    static constexpr std::size_t numRemoveThreads = numInsertThreads;
    static constexpr std::size_t numInsert = 5000;
    auto startFut = start.get_future().share();
    for(std::size_t i = 0; i < numInsertThreads; ++i)
    {
      std::promise<void> p;
      ready.push_back(p.get_future());
      done.push_back(std::async(std::launch::async, [id = i, p = std::move(p), &map, startFut]()mutable{
        p.set_value();
        startFut.wait();
        for(std::size_t i = 0; i < numInsert; ++i)
        {
          int elem = id * numInsert + i;
          map.insert({elem, -elem});
        }
      }));
    }
    for(std::size_t i = 0; i < numRemoveThreads; ++i)
    {
      std::promise<void> p;
      ready.push_back(p.get_future());
      done.push_back(std::async(std::launch::async, [id = i, p = std::move(p), &map, startFut]()mutable{
        p.set_value();
        startFut.wait();
        for(std::size_t i = 0; i < numInsert; ++i)
        {
          int elem = id * numInsert + i;
          while(!map.remove(elem));
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
    BOOST_CHECK(map.empty());
  }
}

