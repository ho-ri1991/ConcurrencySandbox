#pragma once

#include <algorithm>
#include <future>
#include <iterator>

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

template <typename Iterator, typename Fn>
void parallel_for_each(Iterator first, Iterator last, Fn fn)
{
  static constexpr int minPerThread = 25;
  auto dist = std::distance(first, last);
  if(dist == 0)
  {
    return;
  }
  if(dist < 2 * minPerThread)
  {
    std::for_each(first, last, fn);
    return;
  }
  auto mid = first;
  std::advance(mid, dist / 2);
  auto fut = std::async([first = mid, last = last, fn = fn]{
    return parallel_for_each(first, last, fn);
  });
  parallel_for_each(first, mid, fn);
  fut.get();
}

namespace detail
{
template <typename Iterator, typename T>
Iterator parallel_find_impl(Iterator first, Iterator last, const T& value, std::atomic<bool>& done)
{
  static constexpr long long minPerThread = 32;
  auto dist = std::distance(first, last);
  try
  {
    if(dist < 2 * minPerThread)
    {
      for(; first != last && !done.load(); ++first)
      {
        if(*first == value)
        {
          if(!done.exchange(true))
          {
            return first;
          }
        }
      }
      return last;
    }
    auto mid = first;
    std::advance(mid, dist / 2);
    auto fut = std::async([mid, last, &value, &done]{
      return parallel_find_impl(mid, last, value, done);
    });
    auto it = parallel_find_impl(first, mid, value, done);
    return it != mid ? it : fut.get();
  }
  catch(...)
  {
    done.store(true);
    throw;
  }
}
}

template <typename Iterator, typename T>
Iterator parallel_find(Iterator first, Iterator last, const T& value)
{
  std::atomic<bool> done(false);
  return detail::parallel_find_impl(first, last, value, done);
}

template <typename Iterator>
void parallel_partial_sum(Iterator first, Iterator last)
{
  static constexpr int minPerThread = 32;
  int dist = std::distance(first, last);
  int numThreads = (dist + minPerThread - 1) / minPerThread;
  int hardwareNumThreads = std::thread::hardware_concurrency();
  numThreads = std::min(numThreads, hardwareNumThreads == 0 ? 2 : hardwareNumThreads);
  using value_type = typename std::iterator_traits<Iterator>::value_type;
  using return_type = decltype(std::declval<const value_type&>() + std::declval<const value_type&>());
  std::vector<jthread> threads;
  threads.reserve(numThreads - 1);
  std::vector<std::future<return_type>> futures;
  futures.reserve(numThreads - 1);
  std::vector<std::promise<return_type>> promises(numThreads - 1);
  auto blockSize = dist / numThreads;
  auto process = [](Iterator first, Iterator last, std::future<return_type>* prev, std::promise<return_type>* next){
    auto end = last;
    std::advance(end, 1);
    try
    {
      {
        auto it = first;
        auto prev = first;
        it++;
        for(; it != end; it++, prev++)
        {
          *it += *prev;
        }
      }
      return_type prevVal{};
      if(prev)
      {
        prevVal = prev->get();
        *last += prevVal;
      }
      if(next)
      {
        next->set_value(*last);
      }
      if(prev)
      {
        for(; first != last; ++first)
        {
          *first += prevVal;
        }
      }
    }
    catch(...)
    {
      if(next)
      {
        next->set_exception(std::current_exception());
      }
      else
      {
        throw;
      }
    }
  };
  for(int i = 0; i < numThreads - 1; ++i)
  {
    auto it = first;
    std::advance(it, blockSize - 1);
    futures.push_back(promises[i].get_future());
    threads.emplace_back(process, first, it, i == 0 ? nullptr : &futures[i - 1], &promises[i]);
    std::advance(it, 1);
    first = it;
  }
  process(first, std::prev(last), 1 < numThreads ? &futures.back() : nullptr, nullptr);
}

