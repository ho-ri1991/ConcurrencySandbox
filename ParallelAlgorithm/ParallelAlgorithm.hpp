#pragma once

#include <algorithm>
#include <future>
#include <iterator>

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

