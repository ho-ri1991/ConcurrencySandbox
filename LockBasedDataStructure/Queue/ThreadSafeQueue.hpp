#pragma once
#include <mutex>
#include <memory>
#include <condition_variable>
#include <queue>

template <typename T>
class ThreadSafeQueue
{
private:
  struct Node
  {
    std::shared_ptr<T> mData;
    std::unique_ptr<Node> mNext;
  };
  std::mutex mHeadLock;
  std::unique_ptr<Node> mHead;
  std::mutex mTailLock;
  Node* mTail;
private:
  template <typename U>
  void pushImpl(U&& val);
  Node* getTail();
  std::unique_ptr<T> popHead();
public:
  std::shared_ptr<T> tryPop();
  void push(T&& val);
  void push(const T& val);
};

template <typename T>
typename ThreadSafeQueue<T>::Node* ThreadSafeQueue<T>::getTail()
{
  std::lock_guard lk(mTailLock);
  return mTail;
}
template <typename T>
std::unique_ptr<T> ThreadSafeQueue<T>::popHead()
{
  std::lock_guard lk(mHeadLock);
  auto tail = getTail();
  if(mHead.get() == tail)
  {
    return nullptr;
  }
  auto oldHead = std::move(mHead);
  mHead = std::move(oldHead->mNext);
  return oldHead;
}
template <typename T>
std::shared_ptr<T> ThreadSafeQueue<T>::tryPop()
{
  auto res = popHead();
  return res ? res->mData : nullptr;
}
template <typename T>
template <typename U>
void ThreadSafeQueue<T>::pushImpl(U&& val)
{
  auto data = std::make_shared<T>(std::forward<U>(val));
  auto newNode = std::make_unique<Node>();
  std::lock_guard lk(mTailLock);
  mTail->mData = std::move(data);
  mTail->next = std::move(newNode);
  mTail = mTail->next.get();
}
template <typename T>
void ThreadSafeQueue<T>::push(T&& val)
{
  pushImpl(std::move(val));
}
template <typename T>
void ThreadSafeQueue<T>::push(const T& val)
{
  pushImpl(val);
}

