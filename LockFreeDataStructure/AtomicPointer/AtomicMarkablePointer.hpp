#pragma once

#include <atomic>
#include <cstdint>
#include <cassert>
#include <utility>

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
