#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <string>
#include "Future.hpp"

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

BOOST_AUTO_TEST_CASE(Future_int)
{
  {
    using namespace std::literals::chrono_literals;
    my::Promise<int> promise;
    auto fut = promise.getFuture();
    jthread t([p = std::move(promise)]() mutable {
      std::this_thread::sleep_for(100ms);
      p.setValue(42);
    });
    BOOST_CHECK_EQUAL(fut.get(), 42);
  }
  {
    using namespace std::literals::chrono_literals;
    my::Promise<int> promise;
    auto fut = promise.getFuture();
    jthread t([p = std::move(promise)]() mutable {
      std::this_thread::sleep_for(100ms);
      try
      {
        throw std::runtime_error("exception!");
      }
      catch(...)
      {
        p.setException(std::current_exception());
      }
    });
    BOOST_CHECK_EXCEPTION(fut.get(), std::runtime_error, [](const std::exception& ex){ return ex.what() == std::string("exception!"); });
  }
}

BOOST_AUTO_TEST_CASE(Future_string)
{
  {
    using namespace std::literals::chrono_literals;
    my::Promise<std::string> promise;
    auto fut = promise.getFuture();
    jthread t([p = std::move(promise)]() mutable {
      std::this_thread::sleep_for(100ms);
      std::string str("foo");
      p.setValue(str);
    });
    BOOST_CHECK_EQUAL(fut.get(), "foo");
  }
  {
    using namespace std::literals::chrono_literals;
    my::Promise<std::string> promise;
    auto fut = promise.getFuture();
    jthread t([p = std::move(promise)]() mutable {
      std::this_thread::sleep_for(100ms);
      std::string str("foo");
      p.setValue(std::move(str));
    });
    BOOST_CHECK_EQUAL(fut.get(), "foo");
  }
}

