#pragma once

#include <atomic>
#include <array>
#include <memory>
#include <thread>
#include <functional>

class HazardPointerDomain
{
private:
  struct PointerWithThreadID
  {
    std::atomic<std::thread::id> mID;
    std::atomic<void*> mPointer;
    PointerWithThreadID(): mID(std::thread::id()), mPointer(nullptr){}
  };
  std::vector<PointerWithThreadID> mPointers;
  struct ListNode
  {
    void* mData;
    ListNode* mNext;
    std::function<void(void*)> mDeleter;
    template <typename Deleter>
    explicit ListNode(void* data, Deleter deleter): mData(data), mNext(nullptr), mDeleter(deleter) {}
    ~ListNode()
    {
      mDeleter(mData);
    }
  };
  class GlobalDeleteList
  {
  private:
    std::atomic<ListNode*> mHead;
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
    void append(ListNode* node)
    {
      node->mNext = mHead.load();
      while(!mHead.compare_exchange_weak(node->mNext, node));
    }
    ListNode* resetHead()
    {
      return mHead.exchange(nullptr);
    }
    ListNode* loadHead()
    {
      return mHead.load();
    }
  };
  class LocalDeleteList
  {
  private:
    ListNode* mHead;
    std::size_t mSize;
  public:
    LocalDeleteList(): mHead(nullptr), mSize(0) {}
    ~LocalDeleteList()
    {
      if(mHead)
      {
        HazardPointerDomain::sGlobalDeleteList.append(mHead);
      }
    }
    template <typename Deleter>
    void append(void* data, Deleter deleter)
    {
      auto node = std::make_unique<ListNode>(data, deleter);
      append(node.get());
      node.release();
    }
    void append(ListNode* node)
    {
      mSize++;
      node->mNext = mHead;
      mHead = node;
    }
    ListNode* resetHead()
    {
      auto ans = mHead;
      mHead = nullptr;
      mSize = 0;
      return ans;
    }
  };
  class HazardPointerOwner
  {
  private:
    PointerWithThreadID* mPointerWithThreadID;
  public:
    HazardPointerOwner(): mPointerWithThreadID(nullptr)
    {
      for(auto& p: sPointerWithThreadID)
      {
        std::thread::id id;
        if(p.mID.compare_exchange_strong(id, std::this_thread::get_id()))
        {
          mPointerWithThreadID = &p;
          break;
        }
      }
      if(!mPointerWithThreadID)
      {
        throw std::runtime_error("No hazard pointers available");
      }
    }
    ~HazardPointerOwner()
    {
      mPointerWithThreadID->mID.store(std::thread::id());
      mPointerWithThreadID->mPointer.store(nullptr);
    }
    std::atomic<void*>& getPointer()
    {
      return mPointerWithThreadID->mPointer;
    }
  };
  static constexpr std::size_t sArraySize = 64;
  static std::array<PointerWithThreadID, sArraySize> sPointerWithThreadID;
  static GlobalDeleteList sGlobalDeleteList;
  static thread_local LocalDeleteList sLocalDeleteList;
  static thread_local HazardPointerOwner sHazardPointerOwner;
  HazardPointerDomain() = delete;
  HazardPointerDomain(const HazardPointerDomain&) = delete;
  HazardPointerDomain(HazardPointerDomain&&) = delete;
  HazardPointerDomain& operator=(const HazardPointerDomain&) = delete;
  HazardPointerDomain& operator=(HazardPointerDomain&&) = delete;
  ~HazardPointerDomain() = delete;
  static bool isHazardous(void* data);
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

