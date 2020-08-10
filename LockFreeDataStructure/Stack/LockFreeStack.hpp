#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <iostream>

template <typename T, std::size_t Size>
class HazardPointerArray
{
private:
  struct HazardPointer
  {
    std::atomic<std::thread::id> mID;
    std::atomic<T*> mPointer;
    HazardPointer(): mID(std::thread::id()), mPointer(nullptr) {}
  };
  HazardPointer mArray[Size];
  class HazardPointerOwner
  {
  private:
    HazardPointer* mPointer;
  public:
    HazardPointerOwner(HazardPointerArray& arr): mPointer(nullptr)
    {
      for(auto& p: arr.mArray)
      {
        std::thread::id id;
        if(p.mID.compare_exchange_strong(id, std::this_thread::get_id()))
        {
          mPointer = &p;
          break;
        }
      }
      if(!mPointer)
      {
        throw std::runtime_error("No hazard pointers available");
      }
    }
    ~HazardPointerOwner()
    {
      mPointer->mPointer.store(nullptr);
      mPointer->mID.store(std::thread::id());
    }
    std::atomic<T*>& getPointer()
    {
      return mPointer->mPointer;
    }
  };
public:
  std::atomic<T*>& getHazardPointerForCurrentThread()
  {
    static thread_local HazardPointerOwner hpOwner(*this);
    return hpOwner.getPointer();
  }
  bool outstandingHazardPointersFor(T* pointer)
  {
    for(auto& p: mArray)
    {
      if(p.mPointer.load() == pointer)
      {
        return true;
      }
    }
    return false;
  }
};
template <typename T>
class DataToReclaimList
{
private:
  struct Node
  {
    T* mData;
    Node* mNext;
    Node(T* data): mData(data), mNext(nullptr) {}
    ~Node() { delete mData; }
  };
  std::atomic<Node*> mHead;
  void append(Node* node)
  {
    node->mNext = mHead.load();
    while(!mHead.compare_exchange_strong(node->mNext, node));
  }
public:
  void reclaimLater(T* data)
  {
    append(new Node(data));
  }
  template <std::size_t Size>
  void deleteNodesWithoutHazard(HazardPointerArray<T, Size>& arr)
  {
    auto current = mHead.exchange(nullptr);
    while(current)
    {
      auto next = current->mNext;
      if(!arr.outstandingHazardPointersFor(current->mData))
      {
        delete current;
      }
      else
      {
        append(current);
      }
      current = next;
    }
  }
};

template <typename T>
class LockFreeStack
{
private:
  struct Node
  {
    std::shared_ptr<T> mData;
    Node* mNext;
    Node(const T& val): mData(std::make_shared<T>(val)), mNext(nullptr) {}
  };
  std::atomic<Node*> mHead;
  HazardPointerArray<Node, 64> mHazardPointers;
  DataToReclaimList<Node> mDeleteList;
public:
  LockFreeStack();
  ~LockFreeStack();
  void push(const T& val);
  std::shared_ptr<T> pop();
};

template <typename T>
LockFreeStack<T>::LockFreeStack(): mHead(nullptr) {}
template <typename T>
LockFreeStack<T>::~LockFreeStack()
{
  auto node = mHead.load();
  while(node)
  {
    auto next = node->mNext;
    delete node;
    node = next;
  }
}
template <typename T>
void LockFreeStack<T>::push(const T& val)
{
  auto node = new Node(val);
  node->mNext = mHead.load();
  while(!mHead.compare_exchange_weak(node->mNext, node));
}
template <typename T>
std::shared_ptr<T> LockFreeStack<T>::pop()
{
  auto& hazardPointer = mHazardPointers.getHazardPointerForCurrentThread();
  Node* oldHead;
  do
  {
    Node* temp;
    do
    {
      oldHead = mHead.load();
      hazardPointer.store(oldHead);
      temp = mHead.load();
    }
    while(oldHead != temp);
  }
  while(oldHead && !mHead.compare_exchange_strong(oldHead, oldHead->mNext));
  hazardPointer.store(nullptr);
  
  std::shared_ptr<T> ans;
  if(oldHead)
  {
    using std::swap;
    swap(ans, oldHead->mData);
    if(!mHazardPointers.outstandingHazardPointersFor(oldHead))
    {
      delete oldHead;
    }
    else
    {
      mDeleteList.reclaimLater(oldHead);
    }
    mDeleteList.deleteNodesWithoutHazard(mHazardPointers);
  }
  return ans;
}
