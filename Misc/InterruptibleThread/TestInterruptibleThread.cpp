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
#include "InterruptibleThread.hpp"

BOOST_AUTO_TEST_CASE(TestInterruptibleThread)
{
  using namespace std::literals::chrono_literals;
  std::mutex lock;
  std::condition_variable cond;
  bool flag = false;
  auto t = InterruptibleThread([&]{
    try
    {
      while(true)
      {
        interruptionPoint();
      }
    }
    catch(InterruptException& ex) {}

    {
      std::unique_lock lk(lock);
      try { interruptibleWait(lk, cond); }
      catch(InterruptException& ex) {}
    }
    {
      std::unique_lock lk(lock);
      try { interruptibleWait(lk, cond, [&]{ return flag; }); }
      catch(InterruptException& ex) {}
    }
  });
  std::this_thread::sleep_for(100ms);
  t.interrupt();
  std::this_thread::sleep_for(100ms);
  t.interrupt();
  std::this_thread::sleep_for(100ms);
  t.interrupt();
  t.join();
}

