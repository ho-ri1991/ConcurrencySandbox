#pragma once

#include <atomic>
#include <mutex>
#include <list>
#include <shared_mutex>
#include <unordered_map>
#include <optional>
#include <cassert>
#include "Finally.hpp"

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class StripedThreadSafeHashmap
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
  mutable std::vector<std::mutex> mLocks;
  std::atomic<std::size_t> mSize;
  Hash mHasher; // TODO: EBO
  void rehash(std::unique_lock<std::mutex>& lock);
  std::unique_lock<std::mutex> acquire(HashValue hashValue) const;
  std::size_t getBucketIndex(HashValue hashValue) const;
public: StripedThreadSafeHashmap(std::size_t initialBucketSize = 41, const Hash& hasher = Hash());
  std::optional<Value> find(const Key& key) const;
  void addOrUpdate(const Key& key, const Value& val);
  void remove(const Key& key);
};

template <typename Key, typename Value, typename Hash>
std::unique_lock<std::mutex> StripedThreadSafeHashmap<Key, Value, Hash>::acquire(HashValue hashValue) const
{
  std::size_t len = mLocks.size();
  std::size_t index = hashValue % len;
  auto& lock = mLocks[index];
  std::unique_lock<std::mutex> res(lock);
  return res;
}
template <typename Key, typename Value, typename Hash>
std::size_t StripedThreadSafeHashmap<Key, Value, Hash>::getBucketIndex(HashValue hashValue) const
{
  std::size_t len = mBuckets.size();
  return hashValue % len;
}
template <typename Key, typename Value, typename Hash>
void StripedThreadSafeHashmap<Key, Value, Hash>::rehash(std::unique_lock<std::mutex>& lock)
{
  assert(lock.owns_lock());
  std::size_t prevBucketSize = mBuckets.size();
  lock.unlock();
  for(auto& l: mLocks)
  {
    l.lock();
  }
  auto fin = finally([this]{
    for(auto& l: mLocks)
    {
      l.unlock();
    }
  });
  if(prevBucketSize != mBuckets.size())
  {
    return;
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
}
template <typename Key, typename Value, typename Hash>
StripedThreadSafeHashmap<Key, Value, Hash>::StripedThreadSafeHashmap(std::size_t initialBucketSize, const Hash& hasher)
  : mBuckets(initialBucketSize)
  , mLocks(initialBucketSize)
  , mSize(0)
  , mHasher(hasher)
{
}
template <typename Key, typename Value, typename Hash>
std::optional<Value> StripedThreadSafeHashmap<Key, Value, Hash>::find(const Key& key) const
{
  auto hashValue = mHasher(key);
  auto lock = acquire(hashValue);
  std::size_t index = getBucketIndex(hashValue);
  auto& bucket = mBuckets[index];
  auto it = bucket.find(key);
  return it != bucket.end() ? std::optional<Value>(it->second) : std::nullopt;
}
template <typename Key, typename Value, typename Hash>
void StripedThreadSafeHashmap<Key, Value, Hash>::addOrUpdate(const Key& key, const Value& val)
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
void StripedThreadSafeHashmap<Key, Value, Hash>::remove(const Key& key)
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

