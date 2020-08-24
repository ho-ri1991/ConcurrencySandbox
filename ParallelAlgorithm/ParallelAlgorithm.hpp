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

