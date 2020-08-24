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

