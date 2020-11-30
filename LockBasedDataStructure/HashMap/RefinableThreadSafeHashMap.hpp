#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include "Finally.hpp"

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
      sHazardPointerList.append(node.get());
      node.release();
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
  std::atomic<void*>& mHazardPointer;
public:
  explicit HazardPointerHolder(std::atomic<void*>& hazardPointer) noexcept: mHazardPointer(hazardPointer) {}
  ~HazardPointerHolder()
  {
    release();
  }
  void store(void* pointer) noexcept
  {
    mHazardPointer.store(pointer, std::memory_order_seq_cst);
  }
  void release() noexcept
  {
    mHazardPointer.store(nullptr, std::memory_order_seq_cst);
  }
};

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class RefinableThreadSafeHashmap
{
private:
  static constexpr std::size_t sThreshold = 4;
  struct Bucket
  {
    using BucketValue = std::pair<Key, Value>;
    using BucketData = std::list<BucketValue>;
    using BucketIterator = typename BucketData::iterator;
    using ConstBucketIterator = typename BucketData::const_iterator;
    BucketData mBucketData;
    ConstBucketIterator find(const Key& key) const
    {
      return std::find_if(
        mBucketData.begin(),
        mBucketData.end(),
        [&key](auto&& val){ return val.first == key; });
    }
    BucketIterator find(const Key& key)
    {
      return std::find_if(
        mBucketData.begin(),
        mBucketData.end(),
        [&key](auto&& val){ return val.first == key; });
    }
    BucketIterator begin()
    {
      return mBucketData.begin();
    }
    ConstBucketIterator begin() const
    {
      return mBucketData.begin();
    }
    BucketIterator end()
    {
      return mBucketData.end();
    }
    ConstBucketIterator end() const
    {
      return mBucketData.end();
    }
    void append(BucketValue&& value)
    {
      mBucketData.push_back(std::move(value));
    }
    void append(const BucketValue& value)
    {
      mBucketData.push_back(value);
    }
    void remove(BucketIterator it)
    {
      mBucketData.erase(it);
    }
  };
  using HashValue = decltype(std::declval<Hash>()(std::declval<const Key&>()));
  std::vector<Bucket> mBuckets;
  mutable std::atomic<std::vector<std::mutex>*> mLocks;
  std::atomic<bool> mRehash;
  std::atomic<std::size_t> mSize;
  Hash mHasher; // TODO: EBO
  void rehash(std::unique_lock<std::mutex>& lock);
  std::unique_lock<std::mutex> acquire(HashValue hashValue) const;
  std::size_t getBucketIndex(HashValue hashValue) const;
  static void deleter(void* data);
public:
  RefinableThreadSafeHashmap(std::size_t initialBucketSize = 41, const Hash& hash = Hash());
  ~RefinableThreadSafeHashmap();
  std::optional<Value> find(const Key& key) const;
  void addOrUpdate(const Key& key, const Value& val);
  void remove(const Key& key);
};

template <typename Key, typename Value, typename Hash>
void RefinableThreadSafeHashmap<Key, Value, Hash>::deleter(void* data)
{
  delete reinterpret_cast<std::vector<std::mutex>*>(data);
}
template <typename Key, typename Value, typename Hash>
std::unique_lock<std::mutex> RefinableThreadSafeHashmap<Key, Value, Hash>::acquire(HashValue hashValue) const
{
  auto& hp = HazardPointerDomain<>::getHazardPointerForCurrentThread();
  while(true)
  {
    HazardPointerHolder hpHolder(hp);
    std::vector<std::mutex>* locks;
    std::vector<std::mutex>* tmp;
    do
    {
      while(mRehash.load());
      locks = mLocks.load();
      hpHolder.store(locks);
      tmp = mLocks.load();
    }
    while(locks != tmp);
    std::unique_lock lk((*locks)[hashValue % locks->size()]);
    if(!mRehash.load() && locks == mLocks.load())
    {
      return lk;
    }
  }
}
template <typename Key, typename Value, typename Hash>
std::size_t RefinableThreadSafeHashmap<Key, Value, Hash>::getBucketIndex(HashValue hashValue) const
{
  std::size_t len = mBuckets.size();
  return hashValue % len;
}
template <typename Key, typename Value, typename Hash>
void RefinableThreadSafeHashmap<Key, Value, Hash>::rehash(std::unique_lock<std::mutex>& lock)
{
  assert(lock.owns_lock());
  std::size_t prevBucketSize = mBuckets.size();
  lock.unlock();
  while(mRehash.exchange(true));
  if(prevBucketSize != mBuckets.size())
  {
    mRehash.store(false);
    return;
  }
  std::vector<std::mutex>* oldLocks = nullptr;
  {
    oldLocks = mLocks.load();
    for(auto& l: *oldLocks)
    {
      // wait for threads that hold the lock before mRehash is set
      l.lock();
      l.unlock();
    }
    std::size_t newBucketSize = 2 * prevBucketSize;
    std::vector<Bucket> newBucket(newBucketSize);
    for(auto& bucket: mBuckets)
    {
      for(auto& pair: bucket)
      {
        auto hashValue = mHasher(pair.first);
        std::size_t index = hashValue % newBucketSize;
        newBucket[index].append(pair);
      }
    }
    using std::swap;
    swap(mBuckets, newBucket);
    auto newLocks = new std::vector<std::mutex>(newBucketSize);
    mLocks.exchange(newLocks);
    mRehash.store(false);
  }
  HazardPointerDomain<>::retire(oldLocks, &deleter);
}
template <typename Key, typename Value, typename Hash>
RefinableThreadSafeHashmap<Key, Value, Hash>::RefinableThreadSafeHashmap(std::size_t initialBucketSize, const Hash& hasher)
  : mBuckets(initialBucketSize)
  , mLocks(new std::vector<std::mutex>(initialBucketSize))
  , mRehash(false)
  , mSize(0)
  , mHasher(hasher)
{
}
template <typename Key, typename Value, typename Hash>
RefinableThreadSafeHashmap<Key, Value, Hash>::~RefinableThreadSafeHashmap()
{
  auto vec = mLocks.exchange(nullptr);
  delete vec;
}
template <typename Key, typename Value, typename Hash>
std::optional<Value> RefinableThreadSafeHashmap<Key, Value, Hash>::find(const Key& key) const
{
  auto hashValue = mHasher(key);
  auto lock = acquire(hashValue);
  std::size_t index = getBucketIndex(hashValue);
  auto& bucket = mBuckets[index];
  auto it = bucket.find(key);
  return it != bucket.end() ? std::optional<Value>(it->second) : std::nullopt;
}
template <typename Key, typename Value, typename Hash>
void RefinableThreadSafeHashmap<Key, Value, Hash>::addOrUpdate(const Key& key, const Value& val)
{
  auto hashValue = mHasher(key);
  auto lock = acquire(hashValue);
  std::size_t index = getBucketIndex(hashValue);
  auto& bucket = mBuckets[index];
  auto it = bucket.find(key);
  bool doRehash = false;
  if(it == bucket.end())
  {
    bucket.append(std::make_pair(key, val));
    std::size_t curSize = mSize.fetch_add(1);
    doRehash = (curSize / mBuckets.size() > sThreshold);
  }
  else
  {
    it->second = val;
  }
  if(doRehash)
  {
    rehash(lock);
  }
  else
  {
    lock.unlock();
  }
}
template <typename Key, typename Value, typename Hash>
void RefinableThreadSafeHashmap<Key, Value, Hash>::remove(const Key& key)
{
  auto hashValue = mHasher(key);
  auto lock = acquire(hashValue);
  std::size_t index = getBucketIndex(hashValue);
  auto& bucket = mBuckets[index];
  auto it = bucket.find(key);
  if(it != bucket.end())
  {
    bucket.remove(it);
    mSize.fetch_sub(1);
  }
}

