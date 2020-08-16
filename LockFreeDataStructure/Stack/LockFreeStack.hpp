#pragma once

#include <atomic>
#include <array>
#include <memory>
#include <thread>
#include <functional>
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
  std::atomic<Node*> mHead; // operation on this variable have to be memory_order_seq_cst to use hazard pointer
  static void deleteNode(void* node);
public:
  LockFreeStack();
  ~LockFreeStack();
  void push(const T& val);
  std::shared_ptr<T> pop();
};

#include "LockFreeStack.inl"

