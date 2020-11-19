#pragma once

#include <functional>
#include <cassert>
#include <memory>
#include "HazardPointer.hpp"

template <typename T>
class AtomicMarkablePointer
{
private:
  static_assert(1 < alignof(T));
  using InternalRep = std::uintptr_t;
  static constexpr InternalRep sMask = ~static_cast<InternalRep>(1);
  std::atomic<InternalRep> mData;
  static InternalRep zip(T* pointer, bool mark)
  {
    assert((pointer & 1) == 0);
    InternalRep res = reinterpret_cast<InternalRep>(pointer);
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

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LockFreeHashMap
{
private:
  using HashValueType = decltype(std::declval<Hash>()(std::declval<const Key&>()));
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
    Node* p, q;
    do
    {
      auto r1 = markablePointer.load(std::memory_order_seq_cst);
      holder.store(r1.first);
      auto r2 = markablePointer.load(std::memory_order_seq_cst);
      p = r1.first;
      q = r2.first;
    }
    while(p != q);
    if(mark)
    {
      *mark = q.second;
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
    static std::tuple<Node*, Node*, HazardPointerHolder, HazardPointerHolder> find(std::atomic<Node*>& head, HashValueType hashValue, const Key& key)
    {
      using std::swap;
      auto& predHp = HPDomain::getHazardPointerForCurrentThread(0);
      auto& curHp = HPDomain::getHazardPointerForCurrentThread(1);
      auto& succHp = HPDomain::getHazardPointerForCurrentThread(2);
      while(true)
      {
        bool retry = false;
        HazardPointerHolder predHpHolder(predHp);
        HazardPointerHolder curHpHolder(curHp);
        auto* pred = claimPointer(head, predHpHolder);
        auto* cur = claimMarkablePointer(pred->mNext, curHpHolder);
        while(true)
        {
          HazardPointerHolder succHpHolder(succHp);
          bool mark;
          auto* succ = claimMarkablePointer(cur->mNext, succHpHolder, &mark);
          while(mark)
          {
            bool expectedMark = false;
            retry = !pred->mNext.compare_exchange_strong(cur, succ, expectedMark, false);
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
             (hashValue == cur->mHashValue && cur->mValue && cur->mValue->first == key))
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
      while(cur->mHashValue < hashValue || (cur->mHashValue == hashValue && cur->mValue.first != key))
      {
        auto succ = claimMarkablePointer(cur->mNext, succHpHolder, &mark);
        pred = cur;
        swap(predHpHolder, curHpHolder);
        cur = pred;
        swap(curHpHolder, succHpHolder);
      }
      if(cur->mHashValue != hashValue || cur->mValue.first != key || mark)
      {
        return std::nullopt;
      }
      return cur->mValue.second;
    }
    static bool add(std::atomic<Node*>& head, HashValueType hashValue, const Key& key, const Value& value)
    {
      auto newNode = std::make_unique<Node>();
      newNode->mHashValue = hashValue;
      newNode->mValue = std::make_pair(key, value);
      while(true)
      {
        auto [pred, cur, predHpHolder, curHpHolder] = find(head, hashValue, key);
        if(cur->mHashValue == hashValue && cur->mValue.first == key)
        {
          return false;
        }
        newNode->mNext.store(cur, false);
        bool expectedMark = false;
        if(pred->mNext.compare_exchange_strong(cur, newNode.get(), expectedMark, false))
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
        auto [pred, cur, predHpHolder, curHpHolder] = find(head, hashValue, key);
        if(cur->mHashValue != hashValue || cur->mValue.first != key)
        {
          return false;
        }
        auto [succ, mark] = cur->mNext.load();
        mark = false;
        if(!cur->mNext.compare_exchange_strong(succ, succ, mark, false))
        {
          continue;
        }
        mark = false;
        if(pred->mNext.compare_exchange_strong(cur, succ, mark, false))
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
  std::vector<std::atomic<Node*>> mBaseArray;
public:
  LockFreeHashMap(std::size_t baseArraySize = 1 << 13)
    : mList(0, ~static_cast<HashValueType>(0))
    , mBaseArray(baseArraySize)
  {
    for(auto& val: mBaseArray)
    {
      val.store(nullptr, std::memory_order_seq_cst);
    }
    mBaseArray[0].store(mList.mHead.load(std::memory_order_seq_cst));
  }
};

