#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

// https://lwn.net/Articles/267968/
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=314cdbefd1fd0a7acf3780e9628465b77ea6a836
class TicketLock {
private:
  std::atomic<std::uint64_t> mData;
  static constexpr std::uint64_t sNextIncr = (std::uint64_t)1 << 32;
  static constexpr std::uint64_t sOwnerIncr = 1;
  static std::uint64_t zip(std::uint32_t owner, std::uint32_t next) {
    return ((std::uint64_t)next << 32) | (std::uint64_t)owner;
  }
  static std::pair<std::uint32_t, std::uint32_t> unzip(std::uint64_t data) {
    std::uint32_t owner = data & 0xFFFFFFFF;
    std::uint32_t next = data >> 32;
    return {owner, next};
  }

public:
  TicketLock() : mData(0) {}
  void lock() {
    // the memory fence must be acq_rel because
    // t0: mData == 1:0 (next==1, owner==0)
    // t1 thread 1: unlock (next==1, owner==1)
    // t2 thread 2: mData.fetch_add (next==2, owner==1)
    // t3 thread 3: mData.load <- load the value stored by thread 2 not thread 1, needs release sequence
    auto data = mData.fetch_add(sNextIncr, std::memory_order_acq_rel);
    auto [owner, next] = unzip(data);
    if (owner == next) {
      return;
    }
    while (unzip(mData.load(std::memory_order_acquire)).first != next) {
    }
  }
  void unlock() { mData.fetch_add(sOwnerIncr, std::memory_order_release); }
};