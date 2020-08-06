#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <random>
#include "ThreadSafeQueue.hpp"

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

BOOST_AUTO_TEST_CASE(TestThreadSafeQueueWaitAndPop)
{
  ThreadSafeQueue<std::pair<int, int>> queue;
  std::vector<std::pair<int, int>> data1;
  std::vector<std::pair<int, int>> data2;
  {
    jthread push1([&queue](){
      std::random_device rnd;
      std::mt19937 engine(rnd());
      std::uniform_int_distribution<> dist(0, 10);
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(1ms);
      for(int i = 0; i < 100; ++i)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        queue.push(std::make_pair(0, i));
      }
    });
    jthread push2([&queue](){
      std::random_device rnd;
      std::mt19937 engine(rnd());
      std::uniform_int_distribution<> dist(0, 10);
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(1ms);
      for(int i = 0; i < 100; ++i)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        queue.push(std::make_pair(1, i));
      }
    });
    jthread pop1([&queue, &data1](){
      std::random_device rnd;
      std::mt19937 engine(rnd());
      std::uniform_int_distribution<> dist(0, 10);
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(1ms);
      for(std::size_t i = 0; i < 100; ++i)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        data1.push_back(*queue.waitAndPop());
      }
    });
    jthread pop2([&queue, &data2](){
      std::random_device rnd;
      std::mt19937 engine(rnd());
      std::uniform_int_distribution<> dist(0, 10);
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(1ms);
      for(std::size_t i = 0; i < 100; ++i)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        data2.push_back(*queue.waitAndPop());
      }
    });
  }
  {
    std::vector<int> arr1, arr2;
    for(auto& [n, i]: data1)
    {
      if(n == 0)
      {
        arr1.push_back(i);
      }
      else
      {
        arr2.push_back(i);
      }
    }
    for(auto& [n, i]: data2)
    {
      if(n == 0)
      {
        arr1.push_back(i);
      }
      else
      {
        arr2.push_back(i);
      }
    }
    std::sort(arr1.begin(), arr1.end());
    std::sort(arr2.begin(), arr2.end());
    std::vector<int> expected;
    for(std::size_t i = 0; i < 100; ++i)
    {
      expected.push_back(i);
    }
    BOOST_CHECK_EQUAL_COLLECTIONS(arr1.begin(), arr1.end(), expected.begin(), expected.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(arr2.begin(), arr2.end(), expected.begin(), expected.end());
  }
  {
    std::vector<int> arr1, arr2;
    for(auto& [n, i]: data1)
    {
      if(n == 0)
      {
        arr1.push_back(i);
      }
      else
      {
        arr2.push_back(i);
      }
    }
    for(int i = 0; i < (int)arr1.size() - 1; ++i)
    {
      BOOST_CHECK(arr1[i] < arr1[i + 1]);
    }
    for(int i = 0; i < (int)arr2.size() - 1; ++i)
    {
      BOOST_CHECK(arr2[i] < arr2[i + 1]);
    }
    arr1.clear();
    arr2.clear();
    for(auto& [n, i]: data2)
    {
      if(n == 0)
      {
        arr1.push_back(i);
      }
      else
      {
        arr2.push_back(i);
      }
    }
    for(int i = 0; i < (int)arr1.size() - 1; ++i)
    {
      BOOST_CHECK(arr1[i] < arr1[i + 1]);
    }
    for(int i = 0; i < (int)arr2.size() - 1; ++i)
    {
      BOOST_CHECK(arr2[i] < arr2[i + 1]);
    }
  }
}

BOOST_AUTO_TEST_CASE(TestThreadSafeQueuetryPop)
{
  ThreadSafeQueue<std::pair<int, int>> queue;
  std::vector<std::pair<int, int>> data1;
  std::vector<std::pair<int, int>> data2;
  {
    jthread push1([&queue](){
      std::random_device rnd;
      std::mt19937 engine(rnd());
      std::uniform_int_distribution<> dist(0, 10);
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(1ms);
      for(int i = 0; i < 100; ++i)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        queue.push(std::make_pair(0, i));
      }
    });
    jthread push2([&queue](){
      std::random_device rnd;
      std::mt19937 engine(rnd());
      std::uniform_int_distribution<> dist(0, 10);
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(1ms);
      for(int i = 0; i < 100; ++i)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        queue.push(std::make_pair(1, i));
      }
    });
    jthread pop1([&queue, &data1](){
      std::random_device rnd;
      std::mt19937 engine(rnd());
      std::uniform_int_distribution<> dist(0, 10);
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(1ms);
      for(std::size_t i = 0; i < 100; ++i)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        std::shared_ptr<std::pair<int, int>> p;
        while(!(p = queue.tryPop()));
        data1.push_back(*p);
      }
    });
    jthread pop2([&queue, &data2](){
      std::random_device rnd;
      std::mt19937 engine(rnd());
      std::uniform_int_distribution<> dist(0, 10);
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(1ms);
      for(std::size_t i = 0; i < 100; ++i)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        std::shared_ptr<std::pair<int, int>> p;
        while(!(p = queue.tryPop()));
        data2.push_back(*p);
      }
    });
  }
  {
    std::vector<int> arr1, arr2;
    for(auto& [n, i]: data1)
    {
      if(n == 0)
      {
        arr1.push_back(i);
      }
      else
      {
        arr2.push_back(i);
      }
    }
    for(auto& [n, i]: data2)
    {
      if(n == 0)
      {
        arr1.push_back(i);
      }
      else
      {
        arr2.push_back(i);
      }
    }
    std::sort(arr1.begin(), arr1.end());
    std::sort(arr2.begin(), arr2.end());
    std::vector<int> expected;
    for(std::size_t i = 0; i < 100; ++i)
    {
      expected.push_back(i);
    }
    BOOST_CHECK_EQUAL_COLLECTIONS(arr1.begin(), arr1.end(), expected.begin(), expected.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(arr2.begin(), arr2.end(), expected.begin(), expected.end());
  }
  {
    std::vector<int> arr1, arr2;
    for(auto& [n, i]: data1)
    {
      if(n == 0)
      {
        arr1.push_back(i);
      }
      else
      {
        arr2.push_back(i);
      }
    }
    for(int i = 0; i < (int)arr1.size() - 1; ++i)
    {
      BOOST_CHECK(arr1[i] < arr1[i + 1]);
    }
    for(int i = 0; i < (int)arr2.size() - 1; ++i)
    {
      BOOST_CHECK(arr2[i] < arr2[i + 1]);
    }
    arr1.clear();
    arr2.clear();
    for(auto& [n, i]: data2)
    {
      if(n == 0)
      {
        arr1.push_back(i);
      }
      else
      {
        arr2.push_back(i);
      }
    }
    for(int i = 0; i < (int)arr1.size() - 1; ++i)
    {
      BOOST_CHECK(arr1[i] < arr1[i + 1]);
    }
    for(int i = 0; i < (int)arr2.size() - 1; ++i)
    {
      BOOST_CHECK(arr2[i] < arr2[i + 1]);
    }
  }
}

