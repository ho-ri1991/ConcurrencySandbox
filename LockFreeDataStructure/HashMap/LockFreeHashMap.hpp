#pragma once

#include <functional>
#include <cassert>
#include <memory>
#include <optional>
#include <variant>
#include <cmath>
#include "HazardPointer.hpp"

template <typename T>
class AtomicMarkablePointer
{
private:
//  static_assert(1 < alignof(T));
  using InternalRep = std::uintptr_t;
  static constexpr InternalRep sMask = ~static_cast<InternalRep>(1);
  std::atomic<InternalRep> mData;
  static InternalRep zip(T* pointer, bool mark)
  {
    InternalRep res = reinterpret_cast<InternalRep>(pointer);
    assert((res & 1) == 0);
    return res | (mark ? 1 : 0);
  }
  static std::pair<T*, bool> unzip(InternalRep data)
  {
    T* pointer = reinterpret_cast<T*>(data & sMask);
    bool mark = data & 1;
    return {pointer, mark};
  }
public:
  AtomicMarkablePointer() = default;
  AtomicMarkablePointer(T* pointer, bool mark) noexcept: mData(zip(pointer, mark)) {}
  AtomicMarkablePointer(const AtomicMarkablePointer&) = delete;
  AtomicMarkablePointer& operator=(const AtomicMarkablePointer&) = delete;
  ~AtomicMarkablePointer() = default;
  bool is_lock_free() const volatile noexcept { return mData.is_lock_free(); }
  bool is_lock_free() const noexcept { return mData.is_lock_free(); }
  void store(T* pointer, bool mark, std::memory_order order = std::memory_order_seq_cst) volatile noexcept { mData.store(zip(pointer, mark), order); }
  void store(T* pointer, bool mark, std::memory_order order = std::memory_order_seq_cst) noexcept { mData.store(zip(pointer, mark), order); }
  std::pair<T*, bool> load(std::memory_order order = std::memory_order_seq_cst) const volatile noexcept { return unzip(mData.load(order)); }
  std::pair<T*, bool> load(std::memory_order order = std::memory_order_seq_cst) const noexcept { return unzip(mData.load(order)); }
  std::pair<T*, bool> exchange(T* pointer, bool mark, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
  {
    return unzip(mData.exchange(zip(pointer, mark), order));
  }
  std::pair<T*, bool> exchange(T* pointer, bool mark, std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    return unzip(mData.exchange(zip(pointer, mark), order));
  }
  bool compare_exchange_weak(T*& expected_pointer, T* desired_pointer, bool& expected_mark, bool desired_mark, std::memory_order success, std::memory_order failure) volatile noexcept
  {
    InternalRep expected = zip(expected_pointer, expected_mark);
    InternalRep desired = zip(desired_pointer, desired_mark);
    bool ans = mData.compare_exchange_weak(expected, desired, success, failure);
    auto data = unzip(expected);
    expected_pointer = data.first;
    expected_mark = data.second;
    return ans;
  }
  bool compare_exchange_weak(T*& expected_pointer, T* desired_pointer, bool& expected_mark, bool desired_mark, std::memory_order success, std::memory_order failure) noexcept
  {
    InternalRep expected = zip(expected_pointer, expected_mark);
    InternalRep desired = zip(desired_pointer, desired_mark);
    bool ans = mData.compare_exchange_weak(expected, desired, success, failure);
    auto data = unzip(expected);
    expected_pointer = data.first;
    expected_mark = data.second;
    return ans;
  }
  bool compare_exchange_weak(T*& expected_pointer, T* desired_pointer, bool& expected_mark, bool desired_mark, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
  {
    return compare_exchange_weak(expected_pointer, desired_pointer, expected_mark, desired_mark, order, order);
  }
  bool compare_exchange_weak(T*& expected_pointer, T* desired_pointer, bool& expected_mark, bool desired_mark, std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    return compare_exchange_weak(expected_pointer, desired_pointer, expected_mark, desired_mark, order, order);
  }
  bool compare_exchange_strong(T*& expected_pointer, T* desired_pointer, bool& expected_mark, bool desired_mark, std::memory_order success, std::memory_order failure) volatile noexcept
  {
    InternalRep expected = zip(expected_pointer, expected_mark);
    InternalRep desired = zip(desired_pointer, desired_mark);
    bool ans = mData.compare_exchange_strong(expected, desired, success, failure);
    auto data = unzip(expected);
    expected_pointer = data.first;
    expected_mark = data.second;
    return ans;
  }
  bool compare_exchange_strong(T*& expected_pointer, T* desired_pointer, bool& expected_mark, bool desired_mark, std::memory_order success, std::memory_order failure) noexcept
  {
    InternalRep expected = zip(expected_pointer, expected_mark);
    InternalRep desired = zip(desired_pointer, desired_mark);
    bool ans = mData.compare_exchange_strong(expected, desired, success, failure);
    auto data = unzip(expected);
    expected_pointer = data.first;
    expected_mark = data.second;
    return ans;
  }
  bool compare_exchange_strong(T*& expected_pointer, T* desired_pointer, bool& expected_mark, bool desired_mark, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
  {
    return compare_exchange_strong(expected_pointer, desired_pointer, expected_mark, desired_mark, order, order);
  }
  bool compare_exchange_strong(T*& expected_pointer, T* desired_pointer, bool& expected_mark, bool desired_mark, std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    return compare_exchange_strong(expected_pointer, desired_pointer, expected_mark, desired_mark, order, order);
  }
};

struct DefaultInitializer
{
  template <typename T>
  void operator()(T&&) {}
};
template <typename T, std::size_t BaseArraySize = 1 << 10, typename Initializer = DefaultInitializer>
class LockFreeExtendibleBucket
{
private:
  struct BucketNode
  {
    static constexpr std::size_t sBucketSize = 1 << 10;
    template <typename U>
    using BucketArray = std::array<U, sBucketSize>;
    using LeafNodeElement = T;
    using InnerNodeElement = std::atomic<BucketNode*>;
    using Data = std::variant<BucketArray<LeafNodeElement>, BucketArray<InnerNodeElement>>;
    static constexpr std::size_t LeafNodeIndex = 0;
    static constexpr std::size_t InnerNodeIndex = 1;
    unsigned int mHeight;
    Data mBucket;
    explicit BucketNode(unsigned int height)
      : mHeight(height)
      , mBucket(mHeight == 0 ? Data(std::in_place_index<LeafNodeIndex>) : Data(std::in_place_index<InnerNodeIndex>))
    {
      if(mHeight)
      {
        auto& data = std::get<InnerNodeIndex>(mBucket);
        for(auto& elem: data)
        {
          elem.store(nullptr, std::memory_order_relaxed);
        }
      }
      else
      {
        auto& data = std::get<LeafNodeIndex>(mBucket);
        for(auto& elem: data)
        {
          Initializer()(elem);
        }
      }
    }
  };
  std::atomic<BucketNode*> mRoot;
private:
  static std::size_t pow(std::size_t x, std::size_t y)
  {
    std::size_t ans = 1;
    for(std::size_t i = 0; i < y; ++i)
    {
      ans *= x;
    }
    return ans;
  }
  void cleanupTree(BucketNode* node)
  {
    if(!node)
    {
      return;
    }
    if(node->mHeight)
    {
      auto& bucket = std::get<BucketNode::InnerNodeIndex>(node->mBucket);
      for(auto& node: bucket)
      {
        cleanupTree(node.exchange(nullptr, std::memory_order_acquire));
      }
    }
    delete node;
  }
  T& getImpl(std::size_t i, BucketNode* node)
  {
    auto height = node->mHeight;
    if(height == 0)
    {
      return std::get<BucketNode::LeafNodeIndex>(node->mBucket)[i % BucketNode::sBucketSize];
    }
    auto size = pow(BucketNode::sBucketSize, height);
    auto& data = std::get<BucketNode::InnerNodeIndex>(node->mBucket);
    auto& child = data[i / size];
    auto childNode = child.load(std::memory_order_acquire);
    if(childNode == nullptr)
    {
      auto newChild = std::make_unique<BucketNode>(height - 1);
      BucketNode* expected = nullptr;
      auto success = child.compare_exchange_strong(expected, newChild.get(), std::memory_order_acquire, std::memory_order_release);
      if(success)
      {
        childNode = newChild.release();
      }
      else
      {
        childNode = expected;
      }
    }
    return getImpl(i % size, childNode);
  }
public:
  LockFreeExtendibleBucket()
    : mRoot(new BucketNode(0))
  {
  }
  ~LockFreeExtendibleBucket()
  {
    cleanupTree(mRoot.exchange(nullptr, std::memory_order_acquire));
  }
  bool extend()
  {
    auto prevRoot = mRoot.load();
    auto newHeight = prevRoot->mHeight + 1;
    auto newRoot = std::make_unique<BucketNode>(newHeight);
    std::get<BucketNode::InnerNodeIndex>(newRoot->mBucket)[0].store(prevRoot, std::memory_order_relaxed);
    bool success = mRoot.compare_exchange_strong(
      prevRoot, newRoot.get(), std::memory_order_release, std::memory_order_relaxed);
    if(success)
    {
      newRoot.release();
    }
    return success;
  }
  T& operator[](std::size_t i)
  {
    return getImpl(i, mRoot.load(std::memory_order_acquire));
  }
  std::size_t size() const noexcept
  {
    auto root = mRoot.load(std::memory_order_acquire);
    return pow(BaseArraySize, root->mHeight + 1);
  }
};

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LockFreeHashMap
{
private:
  using HashValueType = decltype(std::declval<Hash>()(std::declval<const Key&>()));
  static_assert(std::is_integral_v<HashValueType>);
  using HPDomain = HazardPointerDomain<3>;
  static constexpr std::size_t HashValueTypeBitWidth = sizeof(HashValueType) * 8;
  static constexpr HashValueType sHiMask = static_cast<HashValueType>(3) << (HashValueTypeBitWidth - 2);
  static constexpr HashValueType sMask = ~sHiMask;
  struct Node
  {
    HashValueType mHashValue;
    std::optional<std::pair<Key, Value>> mValue;
    AtomicMarkablePointer<Node> mNext;
  };
  static Node* claimMarkablePointer(AtomicMarkablePointer<Node>& markablePointer, HazardPointerHolder& holder, bool* mark = nullptr)
  {
    Node *p, *q;
    bool m;
    do
    {
      auto r1 = markablePointer.load(std::memory_order_seq_cst);
      holder.store(r1.first);
      auto r2 = markablePointer.load(std::memory_order_seq_cst);
      p = r1.first;
      q = r2.first;
      m = r2.second;
    }
    while(p != q);
    if(mark)
    {
      *mark = m;
    }
    return p;
  }
  class LockFreeList
  {
  public:
    std::atomic<Node*> mHead;
    LockFreeList(HashValueType minHashValue, HashValueType maxHashValue)
    {
      auto last = std::make_unique<Node>();
      last->mHashValue = maxHashValue;
      last->mNext.store(nullptr, false, std::memory_order_relaxed);
      auto head = std::make_unique<Node>();
      head->mHashValue = minHashValue;
      head->mNext.store(last.release(), false, std::memory_order_relaxed);
      mHead.store(head.release(), std::memory_order_release);
    }
    ~LockFreeList()
    {
      auto cur = mHead.exchange(nullptr, std::memory_order_acquire);
      while(cur)
      {
        auto [next, mark] = cur->mNext.load(std::memory_order_acquire);
        delete cur;
        cur = next;
      }
    }
    static void deleter(void* data)
    {
      delete reinterpret_cast<Node*>(data);
    }
    static std::tuple<Node*, Node*, HazardPointerHolder, HazardPointerHolder> find(std::atomic<Node*>& head, HashValueType hashValue, const Key* key)
    {
      using std::swap;
      HazardPointerHolder predHpHolder(HPDomain::getHazardPointerForCurrentThread(0));
      HazardPointerHolder curHpHolder(HPDomain::getHazardPointerForCurrentThread(1));
      HazardPointerHolder succHpHolder(HPDomain::getHazardPointerForCurrentThread(2));
      while(true)
      {
        bool retry = false;
        auto* pred = claimPointer(head, predHpHolder);
        assert((pred->mHashValue & 3) == 0);
        auto* cur = claimMarkablePointer(pred->mNext, curHpHolder);
        while(true)
        {
          bool mark;
          auto* succ = claimMarkablePointer(cur->mNext, succHpHolder, &mark);
          while(mark)
          {
            bool expectedMark = false;
            assert(cur->mValue);
            retry = !pred->mNext.compare_exchange_strong(
              cur, succ, expectedMark, false, std::memory_order_release, std::memory_order_relaxed);
            if(retry)
            {
              break;
            }
            curHpHolder.store(nullptr);
            HPDomain::retire(cur, &deleter);
            cur = succ;
            swap(curHpHolder, succHpHolder);
            succ = claimMarkablePointer(cur->mNext, succHpHolder, &mark);
          }
          if(retry)
          {
            break;
          }
          if((hashValue < cur->mHashValue) ||
             (hashValue == cur->mHashValue && !cur->mValue) || // sentinel key
             (key && hashValue == cur->mHashValue && cur->mValue && cur->mValue->first == *key))
          {
            return {pred, cur, std::move(predHpHolder), std::move(curHpHolder)};
          }
          pred = cur;
          swap(predHpHolder, curHpHolder);
          cur = succ;
          swap(curHpHolder, succHpHolder);
        }
      }
    }
    static std::optional<Value> get(std::atomic<Node*>& head, HashValueType hashValue, const Key& key)
    {
      // get is wait free
      using std::swap;
      HazardPointerHolder predHpHolder(HPDomain::getHazardPointerForCurrentThread(0));
      HazardPointerHolder curHpHolder(HPDomain::getHazardPointerForCurrentThread(1));
      HazardPointerHolder succHpHolder(HPDomain::getHazardPointerForCurrentThread(2));
      bool mark = false;
      auto pred = claimPointer(head, predHpHolder);
      auto cur = claimMarkablePointer(pred->mNext, curHpHolder, &mark);
      while(cur->mHashValue < hashValue || (cur->mHashValue == hashValue && cur->mValue->first != key))
      {
        auto succ = claimMarkablePointer(cur->mNext, succHpHolder, &mark);
        pred = cur;
        swap(predHpHolder, curHpHolder);
        cur = succ;
        swap(curHpHolder, succHpHolder);
      }
      if(cur->mHashValue != hashValue || cur->mValue->first != key || mark)
      {
        return std::nullopt;
      }
      return cur->mValue->second;
    }
    static bool add(std::atomic<Node*>& head, HashValueType hashValue, const Key& key, const Value& value)
    {
      auto newNode = std::make_unique<Node>();
      newNode->mHashValue = hashValue;
      newNode->mValue = std::make_pair(key, value);
      while(true)
      {
        auto [pred, cur, predHpHolder, curHpHolder] = find(head, hashValue, &key);
        if(cur->mHashValue == hashValue && cur->mValue->first == key)
        {
          return false;
        }
        newNode->mNext.store(cur, false, std::memory_order_relaxed);
        bool expectedMark = false;
        if(pred->mNext.compare_exchange_strong(
            cur, newNode.get(), expectedMark, false,
            std::memory_order_release, std::memory_order_relaxed))
        {
          newNode.release();
          return true;
        }
      }
    }
    static bool remove(std::atomic<Node*>& head, HashValueType hashValue, const Key& key)
    {
      while(true)
      {
        auto [pred, cur, predHpHolder, curHpHolder] = find(head, hashValue, &key);
        if(cur->mHashValue != hashValue || cur->mValue->first != key)
        {
          return false;
        }
        assert(cur->mValue);
        auto [succ, mark] = cur->mNext.load(std::memory_order_acquire);
        mark = false;
        if(!cur->mNext.compare_exchange_strong(
            succ, succ, mark, true, std::memory_order_relaxed, std::memory_order_relaxed))
        {
          continue;
        }
        if(pred->mNext.compare_exchange_strong(
           cur, succ, mark, false, std::memory_order_release, std::memory_order_relaxed))
        {
          curHpHolder.store(nullptr);
          HPDomain::retire(cur, &deleter);
        }
        return true;
      }
    }
  };
  static constexpr HashValueType reverse(HashValueType value)
  {
    HashValueType ans = 0;
    for(std::size_t i = 0; i < HashValueTypeBitWidth; ++i)
    {
      ans <<= 1;
      ans |= (value & 1);
      value >>= 1;
    }
    return ans;
  }
  static constexpr HashValueType makeSentinelKey(HashValueType value)
  {
    return reverse(value & sMask);
  }
  static constexpr HashValueType makeOrdinaryKey(HashValueType value)
  {
    return reverse(value & sMask) | 1;
  }
private:
  LockFreeList mList;
  // TODO: make this a tree structure so that we can extend base array
  std::vector<std::atomic<Node*>> mBuckets;
  std::atomic<unsigned int> mBucketSize;
  std::atomic<unsigned int> mSize;
  Hash mHash; // TODO: EBO
private:
  HashValueType getParentIndex(HashValueType index)
  {
    auto i = mBucketSize.load(std::memory_order_relaxed);
    do
    {
      i >>= 1;
    }
    while(index < i);
    return index - i;
  }
  void insertSentinel(HashValueType index)
  {
    auto parentIndex = getParentIndex(index);
    auto& parent = mBuckets[parentIndex];
    if(parent.load(std::memory_order_acquire) == nullptr)
    {
      insertSentinel(parentIndex);
    }
    auto sentinelKey = makeSentinelKey(index);
    auto newNode = std::make_unique<Node>();
    newNode->mHashValue = sentinelKey;
    Node* sentinelNode;
    while(true)
    {
      auto [pred, cur, predHpHolder, curHpHolder] = LockFreeList::find(parent, sentinelKey, nullptr);
      if(cur->mHashValue == sentinelKey)
      {
        sentinelNode = cur;
        break;
      }
      assert(sentinelKey < cur->mHashValue);
      newNode->mNext.store(cur, false, std::memory_order_relaxed);
      bool expectedMark = false;
      if(pred->mNext.compare_exchange_strong(
          cur, newNode.get(), expectedMark, false, std::memory_order_release, std::memory_order_relaxed))
      {
        sentinelNode = newNode.release();
        break;
      }
    }
    if(mBuckets[index].load(std::memory_order_relaxed) == nullptr)
    {
      mBuckets[index].store(sentinelNode, std::memory_order_release);
    }
  }
  std::atomic<Node*>& getSentinelNode(const Key& key)
  {
    auto hashValue = mHash(key);
    auto index = hashValue % mBucketSize.load(std::memory_order_relaxed);
    auto& sentinel = mBuckets[index];
    if(sentinel.load(std::memory_order_acquire) == nullptr)
    {
      insertSentinel(index);
      return mBuckets[index];
    }
    return sentinel;
  }
  static constexpr std::size_t sThreshold = 4;
public:
  LockFreeHashMap(std::size_t baseArraySize = 1 << 13)
    : mList(0, ~static_cast<HashValueType>(0))
    , mBuckets(baseArraySize)
    , mBucketSize(2)
    , mSize(0)
  {
    for(auto& val: mBuckets)
    {
      val.store(nullptr, std::memory_order_relaxed);
    }
    auto head = mList.mHead.load(std::memory_order_relaxed);
    mBuckets[0].store(head, std::memory_order_release);
  }
  bool insert(const std::pair<Key, Value>& elem)
  {
    auto& sentinel = getSentinelNode(elem.first);
    auto splitOrderedKey = makeOrdinaryKey(mHash(elem.first));
    if(!LockFreeList::add(sentinel, splitOrderedKey, elem.first, elem.second))
    {
      return false;
    }
    auto prevSize = mSize.fetch_add(1, std::memory_order_relaxed);
    auto curBucketSize = mBucketSize.load(std::memory_order_relaxed);
    if(prevSize / curBucketSize > sThreshold && curBucketSize < mBuckets.size())
    {
      mBucketSize.compare_exchange_strong(
        curBucketSize, 2 * curBucketSize, std::memory_order_relaxed, std::memory_order_relaxed);
    }
    return true;
  }
  bool remove(const Key& key)
  {
    auto& sentinel = getSentinelNode(key);
    auto splitOrderedKey = makeOrdinaryKey(mHash(key));
    if(!LockFreeList::remove(sentinel, splitOrderedKey, key))
    {
      return false;
    }
    mSize.fetch_add(-1, std::memory_order_relaxed);
    return true;
  }
  std::optional<Value> find(const Key& key)
  {
    auto& sentinel = getSentinelNode(key);
    auto splitOrderedKey = makeOrdinaryKey(mHash(key));
    return LockFreeList::get(sentinel, splitOrderedKey, key);
  }
  std::size_t size() const noexcept { return mSize.load(std::memory_order_relaxed); }
  bool empty() const noexcept { return mSize.load(std::memory_order_relaxed) == 0; }
};

