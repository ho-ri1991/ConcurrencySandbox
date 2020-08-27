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
#include "ThreadPool.hpp"

BOOST_AUTO_TEST_CASE(TestThreadPool)
{
  using namespace std::literals::chrono_literals;
  ThreadPool pool(16);
  std::vector<std::future<int>> vec;
  std::vector<int> expected;
  for(int i = 0; i < 100; ++i)
  {
    auto fn = [i]{ std::this_thread::sleep_for(10ms); return i; };
    vec.push_back(pool.submit(fn));
    expected.push_back(i);
  }
  std::vector<int> actual;
  for(auto& fut: vec)
  {
    actual.push_back(fut.get());
  }
  BOOST_CHECK_EQUAL_COLLECTIONS(actual.begin(), actual.end(), actual.begin(), actual.end());
}

