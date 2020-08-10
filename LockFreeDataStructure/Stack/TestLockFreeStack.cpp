#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <random>
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
  LockFreeStack<int> stack;
  for(int i = 0; i < 100; ++i)
    stack.push(i);
  for(int i = 0; i < 100; ++i)
  {
    auto p = stack.pop();
    while(!p)
    {
      p = stack.pop();
    }
  }

//  jthread push1([&stack](){
//    for(std::size_t i = 0; i < 100; ++i)
//    {
//      stack.push(i);
//    }
//  });
//  jthread push2([&stack](){
//    for(std::size_t i = 100; i < 200; ++i)
//    {
//      stack.push(i);
//    }
//  });
//  std::promise<std::vector<int>> promise1;
//  auto fut1 = promise1.get_future();
//  jthread pop1([&stack, promise = std::move(promise1)]() mutable {
//    std::vector<int> ans;
//    for(std::size_t i = 0; i < 100; ++i)
//    {
//      std::cout << i << std::endl;
//      auto p = stack.pop();
//      while(!p)
//      {
//        p = stack.pop();
//      }
//      ans.push_back(*p);
//    }
//    promise.set_value(std::move(ans));
//  });
//  std::promise<std::vector<int>> promise2;
//  auto fut2 = promise2.get_future();
//  jthread pop2([&stack, promise = std::move(promise2)]() mutable {
//    std::vector<int> ans;
//    for(std::size_t i = 0; i < 100; ++i)
//    {
//      auto p = stack.pop();
//      while(!p)
//      {
//        p = stack.pop();
//      }
//      ans.push_back(*p);
//    }
//    promise.set_value(std::move(ans));
//  });
//  auto vec1 = fut1.get();
//  auto vec2 = fut2.get();
//  vec1.insert(vec1.end(), vec2.begin(), vec2.end());
//  std::sort(vec1.begin(), vec1.end());
//  std::vector<int> expected;
//  for(std::size_t i = 0; i < 200; ++i)
//  {
//    expected.push_back(i);
//  }
//  BOOST_CHECK_EQUAL_COLLECTIONS(vec1.begin(), vec1.end(), expected.begin(), expected.end());
}

