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
  struct PointerWithThreadID
  {
    std::atomic<std::thread::id> mID;
    std::atomic<void*> mPointer;
    PointerWithThreadID(): mID(std::thread::id()), mPointer(nullptr){}
  };
  struct HazardPointerListNode
  {
    std::atomic<void*> mPointer;
    HazardPointerListNode* mNext;
    HazardPointerListNode(): mPointer(nullptr), mNext(nullptr) {}
  };
  class HazardPointerList
  {
  private:
    std::atomic<HazardPointerListNode*> mHead;
    std::atomic<std::size_t> mSize;
  public:
    HazardPointerList(): mHead(nullptr), mSize(0) {}
    ~HazardPointerList()
    {
      auto cur = mHead.load();
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
      ans.reserve(mSize.load());
      auto cur = mHead.load();
      while(cur)
      {
        ans.push_back(cur->mPointer.load());
        cur = cur->mNext;
      }
      return ans;
    }
    void append(HazardPointerListNode* node)
    {
      node->mNext = mHead.load();
      while(!mHead.compare_exchange_weak(node->mNext, node));
      mSize.fetch_add(1);
    }
    std::size_t size() const noexcept
    {
      return mSize.load();
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
      auto cur = mHead.load();
      while(cur)
      {
        auto next = cur->mNext;
        delete cur;
        cur = next;
      }
    }
    void append(DeleteListNode* head, DeleteListNode* last)
    {
      last->mNext = mHead.load();
      while(!mHead.compare_exchange_weak(last->mNext, head));
    }
    void append(DeleteListNode* node)
    {
      node->mNext = mHead.load();
      while(!mHead.compare_exchange_weak(node->mNext, node));
    }
    DeleteListNode* resetHead()
    {
      return mHead.exchange(nullptr);
    }
    DeleteListNode* loadHead()
    {
      return mHead.load();
    }
  };
  class LocalDeleteList
  {
  private:
    DeleteListNode* mHead;
    std::size_t mSize;
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
    std::size_t size() const noexcept
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
      mPointer->store(nullptr);
    }
    std::atomic<void*>& getPointer()
    {
      return *mPointer;
    }
  };
  static HazardPointerList sHazardPointerList;
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
  std::atomic<Node*> mHead;
  static void deleteNode(void* node);
public:
  LockFreeStack();
  ~LockFreeStack();
  void push(const T& val);
  std::shared_ptr<T> pop();
};

#include "LockFreeStack.inl"

