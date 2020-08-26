#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <random>
#include <mutex>
#include <limits>
#include <condition_variable>
#include <unordered_set>
#include "ParallelAlgorithm.hpp"

BOOST_AUTO_TEST_CASE(TestParallelForEach)
{
  std::vector<long long> input;
  for(std::size_t i = 0; i < 100000; ++i)
  {
    input.push_back(i);
  }
  std::vector<long long> output(input.size());
  parallel_for_each(input.begin(), input.end(), [&output](int i){ output[i] = 2 * i; });
  std::vector<long long> expected;
  for(std::size_t i = 0; i < 100000; ++i)
  {
    expected.push_back(2 * i);
  }
  BOOST_CHECK_EQUAL_COLLECTIONS(output.begin(), output.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(TestParallelFind)
{
  std::size_t len = 10000;
  std::random_device rnd;
  std::mt19937 engine(rnd());
  std::uniform_int_distribution<long long> dist(0, std::numeric_limits<long long>::max());
  for(std::size_t i = 0; i < 10; ++i)
  {
    std::vector<long long> vec(len);
    std::unordered_set<long long> s;
    for(auto& val: vec)
    {
      val = dist(engine);
      s.insert(val);
    }
    BOOST_CHECK_EQUAL(
      *parallel_find(vec.begin(), vec.end(), *s.begin()),
      *std::find(vec.begin(), vec.end(), *s.begin())
    );
    BOOST_CHECK(parallel_find(vec.begin(), vec.end(), -1) == std::find(vec.begin(), vec.end(), -1));
  }
}

BOOST_AUTO_TEST_CASE(TestParallelPartialSum)
{
  std::size_t len = 10000;
  std::random_device rnd;
  std::mt19937 engine(rnd());
  std::uniform_int_distribution<long long> dist(-1000, 1000);
  for(std::size_t i = 0; i < 10; ++i)
  {
    std::vector<long long> vec(len);
    for(auto& val: vec)
    {
      val = dist(engine);
      std::cout << val << ",";
    }
    std::cout << std::endl;
    std::vector<long long> expected;
    std::partial_sum(vec.begin(), vec.end(), std::back_inserter(expected));
    parallel_partial_sum(vec.begin(), vec.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), expected.begin(), expected.end());
  }
}

