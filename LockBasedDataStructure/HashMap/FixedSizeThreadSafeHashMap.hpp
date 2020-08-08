#pragma once

#include <mutex>
#include <list>
#include <shared_mutex>
#include <unordered_map>
#include <optional>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class FixedSizeThreadSafeHashMap
{
private:
  struct Bucket
  {
    using BucketValue = std::pair<Key, Value>;
    using BucketData = std::list<BucketValue>;
    BucketData mBucketData;
    mutable std::shared_mutex mLock;
    using BucketIterator = typename BucketData::iterator;
    using ConstBucketIterator = typename BucketData::const_iterator;
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
  };
  std::vector<Bucket> mBuckets;
  Hash mHasher;
  Bucket& getBucket(const Key& key);
  const Bucket& getBucket(const Key& key) const;
public:
  FixedSizeThreadSafeHashMap(std::size_t bucketSize = 41, const Hash& hasher = Hash());
  std::optional<Value> find(const Key& val) const;
  void addOrUpdate(const Key& key, const Value& val);
  void remove(const Key& key);
  std::unordered_map<Key, Value, Hash> getSnapShot() const;
};

template <typename Key, typename Value, typename Hash>
FixedSizeThreadSafeHashMap<Key, Value, Hash>::FixedSizeThreadSafeHashMap(std::size_t bucketSize, const Hash& hasher):
  mBuckets(bucketSize), mHasher(hasher) {}
template <typename Key, typename Value, typename Hash>
typename FixedSizeThreadSafeHashMap<Key, Value, Hash>::Bucket& FixedSizeThreadSafeHashMap<Key, Value, Hash>::getBucket(const Key& key)
{
  return mBuckets[mHasher(key) % mBuckets.size()];
}
template <typename Key, typename Value, typename Hash>
const typename FixedSizeThreadSafeHashMap<Key, Value, Hash>::Bucket& FixedSizeThreadSafeHashMap<Key, Value, Hash>::getBucket(const Key& key) const
{
  return mBuckets[mHasher(key) % mBuckets.size()];
}
template <typename Key, typename Value, typename Hash>
std::optional<Value> FixedSizeThreadSafeHashMap<Key, Value, Hash>::find(const Key& key) const
{
  auto& bucket = getBucket(key);
  std::shared_lock lk(bucket.mLock);
  auto it = bucket.find(key);
  if(it == bucket.end())
  {
    return std::nullopt;
  }
  return it->second;
}
template <typename Key, typename Value, typename Hash>
void FixedSizeThreadSafeHashMap<Key, Value, Hash>::addOrUpdate(const Key& key, const Value& val)
{
  auto& bucket = getBucket(key);
  std::lock_guard lk(bucket.mLock);
  auto it = bucket.find(key);
  if(it == bucket.end())
  {
    bucket.mBucketData.push_back(std::make_pair(key, val));
  }
  else
  {
    it->second = val;
  }
}
template <typename Key, typename Value, typename Hash>
void FixedSizeThreadSafeHashMap<Key, Value, Hash>::remove(const Key& key)
{
  auto& bucket = getBucket(key);
  std::lock_guard lk(bucket.mLock);
  auto it = bucket.find(key);
  if(it != bucket.end())
  {
    bucket.mBucketData.erase(it);
  }
}
template <typename Key, typename Value, typename Hash>
std::unordered_map<Key, Value, Hash> FixedSizeThreadSafeHashMap<Key, Value, Hash>::getSnapShot() const
{
  using SharedLock = std::unique_lock<std::shared_mutex>;
  std::vector<SharedLock> locks;
  locks.reserve(mBuckets.size());
  for(auto& bucket: mBuckets)
  {
    locks.push_back(SharedLock(bucket.mLock));
  }
  std::unordered_map<Key, Value, Hash> ans;
  for(auto& bucket: mBuckets)
  {
    for(auto& [key, value]: bucket)
    {
      ans.insert(std::make_pair(key, value));
    }
  }
  return ans;
}

