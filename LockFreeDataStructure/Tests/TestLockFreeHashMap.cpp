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
}

