#pragma once

#include <atomic>
#include <cstdint>

template <typename T>
class AtomicStampedPointer
{
  static_assert(sizeof(T*) == 8);
  using InternalRep = std::uintptr_t;
  static constexpr std::uintptr_t sPointerMask = 0x0000FFFFFFFFFFFF;
  std::atomic<InternalRep> mData;
public:
  using StampType = std::uint16_t;
private:
  // TODO: use least significant bits that is available by alignments
  static InternalRep zip(T* pointer, StampType stamp) noexcept
  {
    return static_cast<std::uintptr_t>(stamp) << 48 | (sPointerMask & reinterpret_cast<std::uintptr_t>(pointer));
  }
  static std::pair<T*, StampType> unzip(InternalRep data) noexcept
  {
    auto stamp = static_cast<StampType>(data >> 48);
    auto tmp = sPointerMask & data;
    auto sign = tmp >> 47;
    auto signExtensionMask = (static_cast<std::uintptr_t>(0) - sign) << 48;
    auto pointer = reinterpret_cast<T*>((sPointerMask & data) | signExtensionMask);
    return std::make_pair(pointer, stamp);
  }
public:
  AtomicStampedPointer() = default;
  AtomicStampedPointer(T* data): mData(data) {}
  AtomicStampedPointer(const AtomicStampedPointer&) = delete;
  AtomicStampedPointer(AtomicStampedPointer&&) = delete;
  AtomicStampedPointer& operator=(const AtomicStampedPointer&) = delete;
  AtomicStampedPointer& operator=(AtomicStampedPointer&&) = delete;
  ~AtomicStampedPointer() = default;
public:
  bool is_lock_free() const volatile noexcept { return mData.is_lock_free(); }
  bool is_lock_free() const noexcept { return mData.is_lock_free(); }
  void store(T* desired, StampType stamp, std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    auto data = zip(desired, stamp);
    mData.store(data, order);
  }
  void store(T* desired, StampType stamp, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
  {
    auto data = zip(desired, stamp);
    mData.store(data, order);
  }
  std::pair<T*, StampType> load(std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    auto data = mData.load(order);
    return unzip(data);
  }
  std::pair<T*, StampType> load(std::memory_order order = std::memory_order_seq_cst) volatile noexcept
  {
    auto data = mData.load(order);
    return unzip(data);
  }
  std::pair<T*, StampType> exchange(T* desired, StampType stamp, std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    auto data = mData.exchange(zip(desired, stamp), order);
    return unzip(data);
  }
  std::pair<T*, StampType> exchange(T* desired, StampType stamp, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
  {
    auto data = mData.exchange(zip(desired, stamp), order);
    return unzip(data);
  }
  bool compare_exchange_weak(T*& expected_pointer, T* desired_pointer, StampType& expected_stamp, StampType desired_stamp, std::memory_order success, std::memory_order failure) noexcept
  {
    auto expected = zip(expected_pointer, expected_stamp);
    auto ans = mData.compare_exchange_weak(expected, zip(desired_pointer, desired_stamp), success, failure);
    if(!ans)
    {
      auto pair = unzip(expected);
      expected_pointer = pair.first;
      expected_stamp = pair.second;
    }
    return ans;
  }
  bool compare_exchange_weak(T*& expected_pointer, T* desired_pointer, StampType& expected_stamp, StampType desired_stamp, std::memory_order success, std::memory_order failure) volatile noexcept
  {
    auto expected = zip(expected_pointer, expected_stamp);
    auto ans = mData.compare_exchange_weak(expected, zip(desired_pointer, desired_stamp), success, failure);
    if(!ans)
    {
      auto pair = unzip(expected);
      expected_pointer = pair.first;
      expected_stamp = pair.second;
    }
    return ans;
  }
  bool compare_exchange_weak(T*& expected_pointer, T* desired_pointer, StampType& expected_stamp, StampType desired_stamp, std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    return compare_exchange_weak(expected_pointer, desired_pointer, expected_stamp, desired_stamp, order, order);
  }
  bool compare_exchange_weak(T*& expected_pointer, T* desired_pointer, StampType& expected_stamp, StampType desired_stamp, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
  {
    return compare_exchange_weak(expected_pointer, desired_pointer, expected_stamp, desired_stamp, order, order);
  }
  bool compare_exchange_strong(T*& expected_pointer, T* desired_pointer, StampType& expected_stamp, StampType desired_stamp, std::memory_order success, std::memory_order failure) noexcept
  {
    auto expected = zip(expected_pointer, expected_stamp);
    auto ans = mData.compare_exchange_strong(expected, zip(desired_pointer, desired_stamp), success, failure);
    if(!ans)
    {
      auto pair = unzip(expected);
      expected_pointer = pair.first;
      expected_stamp = pair.second;
    }
    return ans;
  }
  bool compare_exchange_strong(T*& expected_pointer, T* desired_pointer, StampType& expected_stamp, StampType desired_stamp, std::memory_order success, std::memory_order failure) volatile noexcept
  {
    auto expected = zip(expected_pointer, expected_stamp);
    auto ans = mData.compare_exchange_strong(expected, zip(desired_pointer, desired_stamp), success, failure);
    if(!ans)
    {
      auto pair = unzip(expected);
      expected_pointer = pair.first;
      expected_stamp = pair.second;
    }
    return ans;
  }
  bool compare_exchange_strong(T*& expected_pointer, T* desired_pointer, StampType& expected_stamp, StampType desired_stamp, std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    return compare_exchange_strong(expected_pointer, desired_pointer, expected_stamp, desired_stamp, order, order);
  }
  bool compare_exchange_strong(T*& expected_pointer, T* desired_pointer, StampType& expected_stamp, StampType desired_stamp, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
  {
    return compare_exchange_strong(expected_pointer, desired_pointer, expected_stamp, desired_stamp, order, order);
  }
};

