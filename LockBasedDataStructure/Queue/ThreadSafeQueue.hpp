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
  mutable std::mutex mHeadLock;
  std::unique_ptr<Node> mHead;
  mutable std::mutex mTailLock;
  Node* mTail;
  std::condition_variable mCond;
private:
  template <typename U>
  void pushImpl(U&& val);
  Node* getTail();
  std::unique_ptr<Node> popHead();
  std::unique_lock<std::mutex> waitForData();
  std::unique_ptr<Node> waitPopHead();
  std::unique_ptr<Node> waitPopHead(T& val);
  std::unique_ptr<Node> tryPopHead();
  std::unique_ptr<Node> tryPopHead(T& val);
public:
  ThreadSafeQueue();
  std::shared_ptr<T> tryPop();
  bool tryPop(T& val);
  std::shared_ptr<T> waitAndPop();
  void waitAndPop(T& val);
  void push(T&& val);
  void push(const T& val);
  bool empty() const;
};

template <typename T>
ThreadSafeQueue<T>::ThreadSafeQueue(): mHead(std::make_unique<Node>()), mTail(mHead.get()) {}
template <typename T>
typename ThreadSafeQueue<T>::Node* ThreadSafeQueue<T>::getTail()
{
  std::lock_guard lk(mTailLock);
  return mTail;
}
template <typename T>
std::unique_ptr<typename ThreadSafeQueue<T>::Node> ThreadSafeQueue<T>::popHead()
{
  auto oldHead = std::move(mHead);
  mHead = std::move(oldHead->mNext);
  return oldHead;
}
template <typename T>
std::unique_lock<std::mutex> ThreadSafeQueue<T>::waitForData()
{
  std::unique_lock lk(mHeadLock);
  mCond.wait(lk, [this](){ return mHead.get() != getTail(); });
  return lk;
}
template <typename T>
std::unique_ptr<typename ThreadSafeQueue<T>::Node> ThreadSafeQueue<T>::waitPopHead()
{
  auto lk = waitForData();
  return popHead();
}
template <typename T>
std::unique_ptr<typename ThreadSafeQueue<T>::Node> ThreadSafeQueue<T>::waitPopHead(T& val)
{
  auto lk = waitForData();
  // copy data before popHead for exception safety.
  // if popHead is executed before move assignment, the poped value is lost in case where this move assignment throws an exception
  val = std::move(*mHead->mData);
  return popHead();
}
template <typename T>
std::unique_ptr<typename ThreadSafeQueue<T>::Node> ThreadSafeQueue<T>::tryPopHead()
{
  std::lock_guard lk(mHeadLock);
  if(mHead.get() == getTail())
  {
    return nullptr;
  }
  return popHead();
}
template <typename T>
std::unique_ptr<typename ThreadSafeQueue<T>::Node> ThreadSafeQueue<T>::tryPopHead(T& val)
{
  std::lock_guard lk(mHeadLock);
  if(mHead.get() == getTail())
  {
    return nullptr;
  }
  val = std::move(*mHead->mData);
  return popHead();
}
template <typename T>
std::shared_ptr<T> ThreadSafeQueue<T>::tryPop()
{
  auto head = tryPopHead();
  return head ? head->mData : nullptr;
}
template <typename T>
bool ThreadSafeQueue<T>::tryPop(T& val)
{
  auto head = tryPopHead(val);
  return head;
}
template <typename T>
std::shared_ptr<T> ThreadSafeQueue<T>::waitAndPop()
{
  auto head = waitPopHead();
  return head->mData;
}
template <typename T>
void ThreadSafeQueue<T>::waitAndPop(T& val)
{
  auto head = waitPopHead(val);
}
template <typename T>
template <typename U>
void ThreadSafeQueue<T>::pushImpl(U&& val)
{
  auto data = std::make_shared<T>(std::forward<U>(val));
  auto newNode = std::make_unique<Node>();
  {
    std::lock_guard lk(mTailLock);
    mTail->mData = std::move(data);
    mTail->mNext = std::move(newNode);
    mTail = mTail->mNext.get();
  }
  mCond.notify_one();
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
template <typename T>
bool ThreadSafeQueue<T>::empty() const
{
  std::lock_guard lk(mHeadLock);
  return mHead.get() == getTail();
}

