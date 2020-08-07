#pragma once

#include <atomic>
#include <memory>
#include <thread>

template <typename T, std::size_t Size>
class HazardPointerDomain
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
    HazardPointer* mHazardPointer;
  public:
    HazardPointerOwner(HazardPointerDomain& arr): mHazardPointer(nullptr)
    {
      for(auto& p: arr.mArray)
      {
        std::thread::id id;
        if(p.mID.compare_exchange_strong(id, std::this_thread::get_id()))
        {
          mHazardPointer = &p;
          break;
        }
      }
      if(!mHazardPointer)
      {
        throw std::runtime_error("No hazard pointers available");
      }
    }
    ~HazardPointerOwner()
    {
      mHazardPointer->mPointer.store(nullptr);
      mHazardPointer->mID.store(std::thread::id());
    }
    std::atomic<T*>& getPointer()
    {
      return mHazardPointer->mPointer;
    }
  };
  class DataToReclaimList
  {
  public:
    struct Node
    {
      T* mData;
      Node* mNext;
      Node(T* data): mData(data), mNext(nullptr) {}
      ~Node() { delete mData; }
    };
  private:
    std::atomic<Node*> mHead;
  public:
    DataToReclaimList(): mHead(nullptr) {}
    ~DataToReclaimList()
    {
      auto current = mHead.load();
      while(current)
      {
        auto next = current->mNext;
        delete current;
        current = next;
      }
    }
    void append(T* data)
    {
      append(new Node(data));
    }
    void append(Node* node)
    {
      node->mNext = mHead.load();
      while(!mHead.compare_exchange_weak(node->mNext, node));
    }
    Node* getHead()
    {
      return mHead.exchange(nullptr);
    }
  };
  DataToReclaimList mDeleteList;
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
  void reclaimLater(T* data)
  {
    mDeleteList.append(data);
  }
  void deleteNodesWithoutHazard()
  {
    auto current = mDeleteList.getHead();
    while(current)
    {
      auto next = current->mNext;
      if(!outstandingHazardPointersFor(current->mData))
      {
        delete current;
      }
      else
      {
        mDeleteList.append(current);
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
  HazardPointerDomain<Node, 64> mHazardPointerDomain;
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
  auto& hazardPointer = mHazardPointerDomain.getHazardPointerForCurrentThread();
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
    if(!mHazardPointerDomain.outstandingHazardPointersFor(oldHead))
    {
      delete oldHead;
    }
    else
    {
      mHazardPointerDomain.reclaimLater(oldHead);
    }
    mHazardPointerDomain.deleteNodesWithoutHazard();
  }
  return ans;
}

