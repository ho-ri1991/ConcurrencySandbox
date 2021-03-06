#include <future>
#include <shared_mutex>
#include <iostream>
#include <vector>
#include "SharedLock.hpp"

int main()
{
  static constexpr std::size_t numThread = 16;
  static constexpr std::size_t numIncr = 10000;
  std::size_t count = 0;
  SharedLock lock;
  {
    std::promise<void> start;
    auto fut = start.get_future().share();
    std::vector<std::future<void>> ready;
    std::vector<std::future<void>> done;
    for(std::size_t i = 0; i < numThread/4; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      done.push_back(std::async(std::launch::async, [p = std::move(promise), start = fut, &lock, &count]()mutable{
        p.set_value();
        start.wait();
        for(std::size_t i = 0; i < numIncr; ++i)
        {
          std::lock_guard lk(lock);
          count++;
        }
      }));
    }
    for(std::size_t i = numThread/4; i < numThread; ++i)
    {
      std::promise<void> promise;
      ready.push_back(promise.get_future());
      done.push_back(std::async(std::launch::async, [p = std::move(promise), start = fut, &lock, &count]()mutable{
        p.set_value();
        start.wait();
        std::size_t c = 0;
        for(std::size_t i = 0; i < numIncr; ++i)
        {
          std::shared_lock lk(lock);
          c = count;
        }
      }));
    }
    for(auto& fut: ready)
    {
      fut.wait();
    }
    start.set_value();
    for(auto& fut: done)
    {
      fut.wait();
    }
  }
  std::cout << count << std::endl;
}

