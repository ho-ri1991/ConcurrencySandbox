#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <random>
#include "Latch.hpp"

class jthread
{
private:
  std::thread t;
public:
  template <typename F, typename... Args>
  jthread(F&& f, Args&&... args): t(std::forward<F>(f), std::forward<Args>(args)...)
  {
    static_assert(std::is_invocable_v<F, Args&&...>);
  }
  jthread(const jthread&) = delete;
  jthread(jthread&&) = default;
  jthread& operator=(const jthread) = delete;
  jthread& operator=(jthread&&) = default;
  void join()
  {
    t.join();
  }
  ~jthread() noexcept
  {
    if(t.joinable())
    {
      t.join();
    }
  }
};

BOOST_AUTO_TEST_CASE(Latch)
{
  constexpr std::size_t threadCount = 10;
  my::Latch latch(threadCount + 1);
  std::vector<jthread> threads;
  threads.reserve(threadCount);
  std::vector<int> data(threadCount);
  std::random_device rnd;
  std::mt19937 engine(rnd());
  std::uniform_int_distribution<> dist(1, 10);
  for(std::size_t i = 0; i < threadCount; ++i)
  {
    auto t = dist(engine);
    threads.emplace_back([i, t, &data, &latch](){
      std::this_thread::sleep_for(std::chrono::milliseconds(t));
      data[i] = i * 42;
      latch.wait();
    });
  }
  latch.wait();
  std::vector<int> expected;
  for(std::size_t i = 0; i < threadCount; ++i)
  {
    expected.push_back(i * 42);
  }
  BOOST_CHECK_EQUAL_COLLECTIONS(data.begin(), data.end(), expected.begin(), expected.end());
}

