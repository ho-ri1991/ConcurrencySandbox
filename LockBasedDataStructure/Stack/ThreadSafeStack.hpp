#pragma once
#include <stack>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <type_traits>

template <typename T>
class ThreadSafeStack
{
private:
  mutable std::mutex mLock;
  std::condition_variable mCond;
  std::stack<std::unique_ptr<T>> mStack;
private:
  template <typename U>
  void pushImpl(U&& val);
public:
  ThreadSafeStack() = default;
  ThreadSafeStack(const ThreadSafeStack& other);
  bool empty() const;
  std::unique_ptr<T> pop();
  std::unique_ptr<T> try_pop();
  void push(const T& val);
  void push(T&& val);
};

template <typename T>
ThreadSafeStack<T>::ThreadSafeStack(const ThreadSafeStack& other)
{
  std::lock_guard lk(other.mLock);
  mStack = other.mStack;
}
template <typename T>
bool ThreadSafeStack<T>::empty() const
{
  std::lock_guard lk(mLock);
  return mStack.empty();
}
template <typename T>
std::unique_ptr<T> ThreadSafeStack<T>::pop()
{
  std::unique_lock lk(mLock);
  mCond.wait(lk, [this](){ return !mStack.empty(); });
  auto ans = std::move(mStack.top());
  mStack.pop();
  return ans;
}
template <typename T>
std::unique_ptr<T> ThreadSafeStack<T>::try_pop()
{
  std::unique_lock lk(mLock);
  if(mStack.empty())
  {
    return nullptr;
  }
  auto ans = std::move(mStack.top());
  mStack.pop();
  return ans;
}
template <typename T>
template <typename U>
void ThreadSafeStack<T>::pushImpl(U&& val)
{
  auto elem = std::make_unique<T>(std::forward<U>(val));
  std::unique_lock lk(mLock);
  mStack.push(std::move(elem));
  mCond.notify_one();
}
template <typename T>
void ThreadSafeStack<T>::push(const T& val)
{
  pushImpl(val);
}
template <typename T>
void ThreadSafeStack<T>::push(T&& val)
{
  pushImpl(std::move(val));
}

