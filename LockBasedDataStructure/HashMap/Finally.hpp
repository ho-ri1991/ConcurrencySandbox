#pragma once

template <typename Fn>
class Finally
{
private:
  Fn fn;
public:
  Finally(Fn fn): fn(fn) {}
  Finally(const Finally&) = delete;
  Finally& operator=(const Finally&) = delete;
  ~Finally() { fn(); }
};
template <typename Fn>
Finally<Fn> finally(Fn fn)
{
  return Finally<Fn>(fn);
}

