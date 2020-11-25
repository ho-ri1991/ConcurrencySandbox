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

BOOST_AUTO_TEST_CASE(TestLockFreeHashMap)
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
    }
  }
  {
    LockFreeHashMap<int, int> map;
    map.insert({42, 42});
    map.find(42);
    map.remove(42);
  }
}

