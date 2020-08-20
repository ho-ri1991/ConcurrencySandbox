#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>

class HazardPointerDomain
{
private:
  struct HazardPointerListNode
  {
    std::atomic<void*> mPointer;
    HazardPointerListNode* mNext;
    HazardPointerListNode(): mPointer(nullptr), mNext(nullptr) {}
  };
  class HazardPointerList
  {
  private:
    std::atomic<HazardPointerListNode*> mHead; // operation on mHead have to be memory_order_seq_cst to get the latest value by load operation, otherwise some threads may miss hazard pointer of another thread by reading old head.
    std::atomic<int> mSize; // this is a supplementary variable to determine when to reclaim memory.
  public:
    HazardPointerList(): mHead(nullptr), mSize(0) {}
    ~HazardPointerList()
    {
      auto cur = mHead.load(std::memory_order_seq_cst);
      if(cur)
      {
        while(cur)
        {
          auto next = cur->mNext;
          delete cur;
          cur = next;
        }
      }
    }
    std::vector<void*> getPointers()
    {
      std::vector<void*> ans;
      ans.reserve(mSize.load(std::memory_order_relaxed));
      auto cur = mHead.load(std::memory_order_seq_cst);
      while(cur)
      {
        ans.push_back(cur->mPointer.load(std::memory_order_seq_cst));
        cur = cur->mNext;
      }
      return ans;
    }
    void append(HazardPointerListNode* node)
    {
      node->mNext = mHead.load(std::memory_order_relaxed);
      mSize.fetch_add(1, std::memory_order_relaxed);
      while(!mHead.compare_exchange_weak(node->mNext, node, std::memory_order_seq_cst, std::memory_order_relaxed));
    }
    int size() const noexcept
    {
      return mSize.load(std::memory_order_relaxed);
    }
  };
  struct DeleteListNode
  {
    void* mData;
    DeleteListNode* mNext;
    std::function<void(void*)> mDeleter;
    template <typename Deleter>
    explicit DeleteListNode(void* data, Deleter deleter): mData(data), mNext(nullptr), mDeleter(deleter) {}
    ~DeleteListNode()
    {
      mDeleter(mData);
    }
  };
  class GlobalDeleteList
  {
  private:
    std::atomic<DeleteListNode*> mHead;
  public:
    GlobalDeleteList(): mHead(nullptr) {}
    ~GlobalDeleteList()
    {
      auto cur = resetHead();
      while(cur)
      {
        auto next = cur->mNext;
        delete cur;
        cur = next;
      }
    }
    void append(DeleteListNode* head, DeleteListNode* last)
    {
      last->mNext = mHead.load(std::memory_order_relaxed);
      while(!mHead.compare_exchange_weak(last->mNext, head, std::memory_order_release, std::memory_order_relaxed));
    }
    void append(DeleteListNode* node)
    {
      append(node, node);
    }
    DeleteListNode* resetHead()
    {
      return mHead.exchange(nullptr, std::memory_order_acquire);
    }
    DeleteListNode* loadHead(std::memory_order order)
    {
      return mHead.load(order);
    }
  };
  class LocalDeleteList
  {
  private:
    DeleteListNode* mHead;
    int mSize;
  public:
    LocalDeleteList(): mHead(nullptr), mSize(0) {}
    ~LocalDeleteList()
    {
      if(mHead)
      {
        auto last = mHead;
        while(last->mNext)
        {
          last = last->mNext;
        }
        HazardPointerDomain::sGlobalDeleteList.append(mHead, last);
      }
    }
    template <typename Deleter>
    void append(void* data, Deleter deleter)
    {
      auto node = std::make_unique<DeleteListNode>(data, deleter);
      append(node.get());
      node.release();
    }
    void append(DeleteListNode* node)
    {
      mSize++;
      node->mNext = mHead;
      mHead = node;
    }
    DeleteListNode* resetHead()
    {
      auto ans = mHead;
      mHead = nullptr;
      mSize = 0;
      return ans;
    }
    int size() const noexcept
    {
      return mSize;
    }
  };
  class HazardPointerOwner
  {
  private:
    std::atomic<void*>* mPointer;
  public:
    HazardPointerOwner(): mPointer(nullptr)
    {
      auto node = std::make_unique<HazardPointerListNode>();
      mPointer = &node->mPointer;
      sHazardPointerList.append(node.get());
      node.release();
    }
    ~HazardPointerOwner()
    {
      mPointer->store(nullptr, std::memory_order_seq_cst);
    }
    std::atomic<void*>& getPointer()
    {
      return *mPointer;
    }
  };
  static HazardPointerList sHazardPointerList;
  // TODO: global delete list have to be destructed after local delete list because the destructor of local delete list moves its nodes to the global list. We cannot delete the nodes in the local list in general because other threads may be still using the node.
  static GlobalDeleteList sGlobalDeleteList;
  static thread_local LocalDeleteList sLocalDeleteList;
  static thread_local HazardPointerOwner sHazardPointerOwner;
  HazardPointerDomain() = delete;
  HazardPointerDomain(const HazardPointerDomain&) = delete;
  HazardPointerDomain(HazardPointerDomain&&) = delete;
  HazardPointerDomain& operator=(const HazardPointerDomain&) = delete;
  HazardPointerDomain& operator=(HazardPointerDomain&&) = delete;
  ~HazardPointerDomain() = delete;
  template <typename Deleter>
  static void appendToLocalDeleteList(void* data, Deleter deleter);
public:
  static std::atomic<void*>& getHazardPointerForCurrentThread();
  template <typename Deleter>
  static void retire(void* data, Deleter deleter);
  static void tryDeallocateLocalList();
};
class HazardPointerHolder
{
private:
  std::atomic<void*>& mHazardPointer;
public:
  explicit HazardPointerHolder(std::atomic<void*>& hazardPointer): mHazardPointer(hazardPointer) {}
  ~HazardPointerHolder()
  {
    release();
  }
  void store(void* pointer)
  {
    mHazardPointer.store(pointer);
  }
  void release()
  {
    mHazardPointer.store(nullptr);
  }
};

template<typename T>
class MSQueue
{
private:
  struct Node
  {
    std::atomic<T*> mData;
    std::atomic<Node*> mNext;
    Node(): mData(nullptr), mNext(nullptr) {}
  };
  std::atomic<Node*> mHead;
  std::atomic<Node*> mTail;
  static void deleteNode(void* node);
public:
  MSQueue();
  ~MSQueue();
  MSQueue(const MSQueue&) = delete;
  MSQueue(MSQueue&&) = delete;
  MSQueue& operator=(const MSQueue&) = delete;
  MSQueue& operator=(MSQueue&&) = delete;
  void push(const T& data);
  std::unique_ptr<T> tryPop();
};

template <typename T>
void MSQueue<T>::deleteNode(void* node)
{
  delete reinterpret_cast<Node*>(node);
}

template <typename T>
MSQueue<T>::MSQueue(): mHead(new Node()), mTail(mHead.load()) {}

template <typename T>
MSQueue<T>::~MSQueue()
{
  auto node = mHead.load();
  while(node)
  {
    auto next = node->mNext.load();
    delete node->mData.load();
    delete node;
    node = next;
  }
}

template <typename T>
void MSQueue<T>::push(const T& data)
{
  HazardPointerHolder hp(HazardPointerDomain::getHazardPointerForCurrentThread());
  auto p = std::make_unique<T>(data);
  auto node = std::make_unique<Node>();
  while(true)
  {
    Node* tail = nullptr;
    Node* tmp = nullptr;
    do
    {
      tail = mTail.load();
      hp.store(tail);
      tmp = mTail.load();
    }
    while(tail != tmp);
    T* expected = nullptr;
    if(tail->mData.compare_exchange_strong(expected, p.get()))
    {
      p.release();
      Node* expected = nullptr;
      Node* newNext = node.get();
      if(tail->mNext.compare_exchange_strong(expected, node.get()))
      {
        node.release();
      }
      else
      {
        newNext = expected;
      }
      mTail.compare_exchange_strong(tail, newNext);
      break;
    }
    else
    {
      Node* expected = nullptr;
      Node* newNext = node.get();
      if(tail->mNext.compare_exchange_strong(expected, node.get()))
      {
        node.release();
        node = std::make_unique<Node>();
      }
      else
      {
        newNext = expected;
      }
      mTail.compare_exchange_strong(tail, newNext);
    }
  }
}

template <typename T>
std::unique_ptr<T> MSQueue<T>::tryPop()
{
  HazardPointerHolder hp(HazardPointerDomain::getHazardPointerForCurrentThread());
  while(true)
  {
    Node* head = nullptr;
    Node* tmp = nullptr;
    do
    {
      head = mHead.load();
      hp.store(head);
      if(head == mTail.load())
      {
        return nullptr;
      }
      tmp = mHead.load();
    }
    while(head != tmp);
    auto next = head->mNext.load();
    if(mHead.compare_exchange_strong(head, next))
    {
      std::unique_ptr<T> ans(head->mData);
      hp.release();
      HazardPointerDomain::retire(head, &deleteNode);
      return ans;
    }
  }
}

#include "MSQueue.inl"

