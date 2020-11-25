#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace Detail
{
struct HazardPointerListNode
{
  std::atomic<void*> mPointer;
  HazardPointerListNode* mNext;
  HazardPointerListNode() noexcept: mPointer(nullptr), mNext(nullptr) {}
};
class HazardPointerList
{
private:
  std::atomic<HazardPointerListNode*> mHead; // operation on mHead have to be memory_order_seq_cst to get the latest value by load operation, otherwise some threads may miss hazard pointer of another thread by reading old head.
  std::atomic<int> mSize; // this is a supplementary variable to determine when to reclaim memory.
public:
  HazardPointerList() noexcept: mHead(nullptr), mSize(0) {}
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
  std::vector<void*> getPointers() const
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
  explicit DeleteListNode(void* data, Deleter&& deleter): mData(data), mNext(nullptr), mDeleter(std::forward<Deleter>(deleter)) {}
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
  GlobalDeleteList() noexcept: mHead(nullptr) {}
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
  void append(DeleteListNode* head, DeleteListNode* last) noexcept
  {
    last->mNext = mHead.load(std::memory_order_relaxed);
    while(!mHead.compare_exchange_weak(last->mNext, head, std::memory_order_release, std::memory_order_relaxed));
  }
  void append(DeleteListNode* node) noexcept
  {
    append(node, node);
  }
  DeleteListNode* resetHead() noexcept
  {
    return mHead.exchange(nullptr, std::memory_order_acquire);
  }
  DeleteListNode* loadHead(std::memory_order order) noexcept
  {
    return mHead.load(order);
  }
};
class LocalDeleteList
{
private:
  DeleteListNode* mHead;
  int mSize;
  GlobalDeleteList& mGlobalList;
public:
  LocalDeleteList(GlobalDeleteList& globalList) noexcept: mHead(nullptr), mSize(0), mGlobalList(globalList) {}
  ~LocalDeleteList()
  {
    if(mHead)
    {
      auto last = mHead;
      while(last->mNext)
      {
        last = last->mNext;
      }
      mGlobalList.append(mHead, last);
    }
  }
  template <typename Deleter>
  void append(void* data, Deleter&& deleter)
  {
    auto node = std::make_unique<DeleteListNode>(data, std::forward<Deleter>(deleter));
    append(node.get());
    node.release();
  }
  void append(DeleteListNode* node) noexcept
  {
    mSize++;
    node->mNext = mHead;
    mHead = node;
  }
  DeleteListNode* resetHead() noexcept
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
}

template <std::size_t HazardPointerNumPerThread = 1>
class HazardPointerDomain
{
private:
  class HazardPointerOwner
  {
  private:
    std::atomic<void*>* mPointer;
  public:
    HazardPointerOwner(): mPointer(nullptr)
    {
      auto node = std::make_unique<Detail::HazardPointerListNode>();
      mPointer = &node->mPointer;
      sHazardPointerList.append(node.release());
    }
    ~HazardPointerOwner()
    {
      mPointer->store(nullptr, std::memory_order_seq_cst);
    }
    std::atomic<void*>& getPointer() noexcept
    {
      return *mPointer;
    }
  };
  static Detail::HazardPointerList sHazardPointerList;
  // TODO: global delete list have to be destructed after local delete list because the destructor of local delete list moves its nodes to the global list. We cannot delete the nodes in the local list in general because other threads may be still using the node.
  static Detail::GlobalDeleteList sGlobalDeleteList;
  static thread_local Detail::LocalDeleteList sLocalDeleteList;
  static thread_local HazardPointerOwner sHazardPointerOwner[HazardPointerNumPerThread];
  HazardPointerDomain() = delete;
  HazardPointerDomain(const HazardPointerDomain&) = delete;
  HazardPointerDomain(HazardPointerDomain&&) = delete;
  HazardPointerDomain& operator=(const HazardPointerDomain&) = delete;
  HazardPointerDomain& operator=(HazardPointerDomain&&) = delete;
  ~HazardPointerDomain() = delete;
  template <typename Deleter>
  static void appendToLocalDeleteList(void* data, Deleter&& deleter) { sLocalDeleteList.append(data, std::forward<Deleter>(deleter)); }
public:
  static std::atomic<void*>& getHazardPointerForCurrentThread(std::size_t i = 0) noexcept { return sHazardPointerOwner[i].getPointer(); }
  template <typename Deleter>
  static void retire(void* data, Deleter&& deleter)
  {
    appendToLocalDeleteList(data, deleter);
    if(2 * sHazardPointerList.size() < sLocalDeleteList.size())
    {
      tryDeallocateLocalList();
    }
  }
  static void tryDeallocateLocalList()
  {
    // we do not have to collect nodes in the global list right away.
    if(sGlobalDeleteList.loadHead(std::memory_order_relaxed))
    {
      auto globalHead = sGlobalDeleteList.resetHead();
      if(globalHead)
      {
        auto cur = globalHead;
        while(cur)
        {
          auto next = cur->mNext;
          sLocalDeleteList.append(cur);
          cur = next;
        }
      }
    }
    auto arr = sHazardPointerList.getPointers();
    auto cur = sLocalDeleteList.resetHead();
    while(cur)
    {
      auto next = cur->mNext;
      if(std::find(arr.begin(), arr.end(), cur->mData) == arr.end())
      {
        delete cur;
      }
      else
      {
        sLocalDeleteList.append(cur);
      }
      cur = next;
    }
  }
};
template <std::size_t N>
Detail::HazardPointerList HazardPointerDomain<N>::sHazardPointerList;
template <std::size_t N>
Detail::GlobalDeleteList HazardPointerDomain<N>::sGlobalDeleteList;
template <std::size_t N>
thread_local Detail::LocalDeleteList HazardPointerDomain<N>::sLocalDeleteList(sGlobalDeleteList);
template <std::size_t N>
thread_local typename HazardPointerDomain<N>::HazardPointerOwner HazardPointerDomain<N>::sHazardPointerOwner[N];

class HazardPointerHolder
{
private:
  std::atomic<void*>* mHazardPointer;
public:
  explicit HazardPointerHolder(std::atomic<void*>& hazardPointer) noexcept: mHazardPointer(&hazardPointer) {}
  ~HazardPointerHolder()
  {
    if(mHazardPointer)
    {
      release();
    }
  }
  HazardPointerHolder(const HazardPointerHolder&) = delete;
  HazardPointerHolder(HazardPointerHolder&& other) noexcept: mHazardPointer(other.mHazardPointer)
  {
    other.mHazardPointer = nullptr;
  }
  HazardPointerHolder& operator=(const HazardPointerHolder&) = delete;
  HazardPointerHolder& operator=(HazardPointerHolder&& other) noexcept
  {
    HazardPointerHolder tmp(std::move(other));
    swap(tmp);
    return *this;
  }
  void store(void* pointer) noexcept
  {
    mHazardPointer->store(pointer, std::memory_order_seq_cst);
  }
  void release() noexcept
  {
    mHazardPointer->store(nullptr, std::memory_order_seq_cst);
  }
  void swap(HazardPointerHolder& other)
  {
    using std::swap;
    swap(this->mHazardPointer, other.mHazardPointer);
  }
};

inline void swap(HazardPointerHolder& x, HazardPointerHolder& y)
{
  x.swap(y);
}

template <typename T>
T* claimPointer(std::atomic<T*>& pointer, HazardPointerHolder& holder)
{
  T *p, *q;
  do
  {
    p = pointer.load(std::memory_order_seq_cst);
    holder.store(p);
    q = pointer.load(std::memory_order_seq_cst);
  }
  while (p != q);
  return p;
}
