#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include "Barrier.hpp"
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <random>
#include <string>
#include <thread>

class jthread {
private:
  std::thread t;

public:
  template <typename F, typename... Args>
  jthread(F &&f, Args &&... args)
      : t(std::forward<F>(f), std::forward<Args>(args)...) {
    static_assert(std::is_invocable_v<F, Args &&...>);
  }
  jthread(const jthread &) = delete;
  jthread(jthread &&) = default;
  jthread &operator=(const jthread) = delete;
  jthread &operator=(jthread &&) = default;
  void join() { t.join(); }
  ~jthread() noexcept {
    if (t.joinable()) {
      t.join();
    }
  }
};

BOOST_AUTO_TEST_CASE(Barrier) {
  constexpr std::size_t threadCount = 10;
  constexpr std::size_t numTry = 100;
  my::Barrier barrier(threadCount + 1);
  std::vector<jthread> threads;
  threads.reserve(threadCount);
  std::vector<int> data(threadCount, 0);
  std::random_device rnd;
  std::mt19937 engine(rnd());
  std::uniform_int_distribution<> dist(1, 10);
  for (std::size_t i = 0; i < threadCount; ++i) {
    auto t = dist(engine);
    threads.emplace_back([i, t, &data, &barrier]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(t));
      for (std::size_t j = 0; j < numTry; ++j) {
        data[i]++;
        barrier.wait();
      }
    });
  }
  for (std::size_t j = 0; j < numTry; ++j) {
    barrier.wait();
  }
  std::vector<int> expected;
  for (std::size_t i = 0; i < threadCount; ++i) {
    expected.push_back(100);
  }
  BOOST_CHECK_EQUAL_COLLECTIONS(data.begin(), data.end(), expected.begin(),
                                expected.end());
}
