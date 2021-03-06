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
#include "LockFreeStack.hpp"

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

BOOST_AUTO_TEST_CASE(TestLockFreeStack)
{
  {
    LockFreeStack<int> stack1;
    LockFreeStack<int> stack2;
    std::mutex lock;
    std::condition_variable cond;
    std::atomic<bool> start = false;
    jthread push1([&](){
      while(!start.load());
      for(std::size_t i = 0; i < 1000; ++i)
      {
        stack1.push(i);
      }
    });
    jthread push2([&](){
      while(!start.load());
      for(std::size_t i = 1000; i < 2000; ++i)
      {
        stack1.push(i);
      }
    });
    jthread push3([&](){
      while(!start.load());
      for(std::size_t i = 2000; i < 3000; ++i)
      {
        stack1.push(i);
      }
    });
    jthread push4([&](){
      while(!start.load());
      for(std::size_t i = 3000; i < 4000; ++i)
      {
        stack1.push(i);
      }
    });
    jthread popAndPush1([&](){
      while(!start.load());
      for(std::size_t i = 0; i < 1000; ++i)
      {
        auto p = stack1.pop();
        while(!p)
        {
          p = stack1.pop();
        }
        stack2.push(*p);
      }
    });
    jthread popAndPush2([&](){
      while(!start.load());
      for(std::size_t i = 0; i < 1000; ++i)
      {
        auto p = stack1.pop();
        while(!p)
        {
          p = stack1.pop();
        }
        stack2.push(*p);
      }
    });
    jthread popAndPush3([&](){
      while(!start.load());
      for(std::size_t i = 0; i < 1000; ++i)
      {
        auto p = stack1.pop();
        while(!p)
        {
          p = stack1.pop();
        }
        stack2.push(*p);
      }
    });
    jthread popAndPush4([&](){
      while(!start.load());
      for(std::size_t i = 0; i < 1000; ++i)
      {
        auto p = stack1.pop();
        while(!p)
        {
          p = stack1.pop();
        }
        stack2.push(*p);
      }
    });
    std::promise<std::vector<int>> promise1;
    auto fut1 = promise1.get_future();
    jthread pop1([&stack2, &lock, &cond, &start, promise = std::move(promise1)]() mutable {
      while(!start.load());
      std::vector<int> ans;
      for(std::size_t i = 0; i < 1000; ++i)
      {
        auto p = stack2.pop();
        while(!p)
        {
          p = stack2.pop();
        }
        ans.push_back(*p);
      }
      promise.set_value(std::move(ans));
    });
    std::promise<std::vector<int>> promise2;
    auto fut2 = promise2.get_future();
    jthread pop2([&stack2, &lock, &cond, &start, promise = std::move(promise2)]() mutable {
      while(!start.load());
      std::vector<int> ans;
      for(std::size_t i = 0; i < 1000; ++i)
      {
        auto p = stack2.pop();
        while(!p)
        {
          p = stack2.pop();
        }
        ans.push_back(*p);
      }
      promise.set_value(std::move(ans));
    });
    std::promise<std::vector<int>> promise3;
    auto fut3 = promise3.get_future();
    jthread pop3([&stack2, &lock, &cond, &start, promise = std::move(promise3)]() mutable {
      while(!start.load());
      std::vector<int> ans;
      for(std::size_t i = 0; i < 1000; ++i)
      {
        auto p = stack2.pop();
        while(!p)
        {
          p = stack2.pop();
        }
        ans.push_back(*p);
      }
      promise.set_value(std::move(ans));
    });
    std::promise<std::vector<int>> promise4;
    auto fut4 = promise4.get_future();
    jthread pop4([&stack2, &lock, &cond, &start, promise = std::move(promise4)]() mutable {
      while(!start.load());
      std::vector<int> ans;
      for(std::size_t i = 0; i < 1000; ++i)
      {
        auto p = stack2.pop();
        while(!p)
        {
          p = stack2.pop();
        }
        ans.push_back(*p);
      }
      promise.set_value(std::move(ans));
    });
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1ms);
    start.store(true);
    auto vec1 = fut1.get();
    auto vec2 = fut2.get();
    auto vec3 = fut3.get();
    auto vec4 = fut4.get();
    vec1.insert(vec1.end(), vec2.begin(), vec2.end());
    vec1.insert(vec1.end(), vec3.begin(), vec3.end());
    vec1.insert(vec1.end(), vec4.begin(), vec4.end());
    std::sort(vec1.begin(), vec1.end());
    std::vector<int> expected;
    for(std::size_t i = 0; i < 4000; ++i)
    {
      expected.push_back(i);
    }
    BOOST_CHECK_EQUAL_COLLECTIONS(vec1.begin(), vec1.end(), expected.begin(), expected.end());
  }
}

