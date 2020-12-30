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

template <typename T>
struct sorter
{
  ThreadPool<LockFreeLocalWorkQueue> pool;
  std::list<T> sort(std::list<T>& data)
  {
    using namespace std::literals::chrono_literals;
    if(data.empty())
    {
      return data;
    }
    std::list<T> result;
    result.splice(result.begin(), data, data.begin());
    const T& pivot = *result.begin();
    typename std::list<T>::iterator dividePoint = std::partition(data.begin(), data.end(), [&](const T& val) { return val < pivot; });
    std::list<T> lowerData;
    lowerData.splice(lowerData.end(), data, data.begin(), dividePoint);
    auto fut = pool.submit([&lowerData, this]{ return sort(lowerData);});
    auto higherResult = sort(data);
    result.splice(result.end(), higherResult);
    while(fut.wait_for(0s) == std::future_status::timeout)
    {
      pool.runPendingTask();
    }
    result.splice(result.begin(), fut.get());
    return result;
  }
};
template <typename T>
std::list<T> parallel_quick_sort(std::list<T> input)
{
  if(input.empty())
  {
    return input;
  }
  sorter<T> s;
  return s.sort(input);
}

BOOST_AUTO_TEST_CASE(TestParallelQuickSort)
{
  std::size_t len = 10000;
  std::random_device rnd;
  std::mt19937 engine(rnd());
  std::uniform_int_distribution<int> dist(-1000, 1000);
  for(std::size_t i = 0; i < 10; ++i)
  {
    std::list<int> ls;
    for(std::size_t j = 0; j < len; ++j)
    {
      ls.push_back(dist(engine));
    }
    std::vector<int> expected;
    for(auto val: ls)
    {
      expected.push_back(val);
    }
    std::cout << std::endl;
    std::sort(expected.begin(), expected.end());
    auto actual = parallel_quick_sort(ls);
    BOOST_CHECK_EQUAL_COLLECTIONS(actual.begin(), actual.end(), expected.begin(), expected.end());
  }
}

