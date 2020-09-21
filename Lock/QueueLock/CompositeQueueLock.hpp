#pragma once

#include <atomic>
#include <thread>
#include <random>
#include <chrono>
#include "AtomicStampedPointer.hpp"

template <typename Duration>
class Backoff
{
private:
  Duration mMin;
  Duration mMax;
  Duration mLimit;
  std::mt19937 mRandomEngine;
public:
  Backoff(Duration min, Duration max, std::mt19937 randomEngine): mMin(min), mMax(max), mLimit(mMin), mRandomEngine(std::move(randomEngine)) {}
  void operator()()
  {
    // TODO: use information of Rep and Period
    std::uniform_int_distribution<unsigned long long> dist(0, mLimit.count());
    auto delay = dist(mRandomEngine);
    mLimit = std::min(mMax, 2 * mLimit);
    std::this_thread::sleep_for(Duration(delay));
  }
};

class CompositeQueueLock
{
private:
  using Clock = std::chrono::steady_clock;
  static constexpr std::size_t sArraySize = 16;
  static constexpr std::size_t hardware_destructive_interference_size = 64;
  using BackoffDuration = std::chrono::microseconds;
  static constexpr BackoffDuration sMinBackoffDuration = std::chrono::microseconds(50);
  static constexpr BackoffDuration sMaxBackoffDuration = std::chrono::microseconds(10000);
  enum class State
  {
    Free,
    Waiting,
    Released,
    Aborted
  };
  struct alignas(hardware_destructive_interference_size) Node
  {
    std::atomic<State> mState;
    std::atomic<Node*> mPred;
    Node(): mState(State::Free), mPred(nullptr) {}
  };
  static thread_local Node* sMyNode;
  Node mArray[sArraySize];
  AtomicStampedPointer<Node> mTail;
public:
  CompositeQueueLock(): mTail(nullptr, 0) {}
  Node* getNode(Clock::time_point deadline)
  {
    std::random_device rnd;
    std::mt19937 engine(rnd());
    std::uniform_int_distribution<int> dist(0, sArraySize - 1);
    int index = dist(engine);
    Node* node = &mArray[index];
    Backoff<BackoffDuration> backoff(sMinBackoffDuration, sMaxBackoffDuration, std::move(engine));
    do
    {
      State state = State::Free;
      if(node->mState.compare_exchange_strong(state, State::Waiting))
      {
        return node;
      }
      auto [tail, stamp] = mTail.load();
      if(node == tail && (state == State::Released || state == State::Aborted))
      {
        Node* pred = nullptr;
        if(state == State::Aborted)
        {
          pred = tail->mPred.load();
        }
        if(mTail.compare_exchange_strong(tail, pred, stamp, stamp + 1))
        {
          node->mState.store(State::Waiting);
          return node;
        }
      }
      backoff();
    }
    while(Clock::now() < deadline);
    return nullptr;
  }
  std::pair<Node*, bool> spliceNode(Node* node, Clock::time_point deadline)
  {
    auto [tail, stamp] = mTail.load();
    do
    {
      if(mTail.compare_exchange_strong(tail, node, stamp, stamp + 1))
      {
        return {tail, true};
      }
    }
    while(Clock::now() < deadline);
    node->mState.store(State::Free);
    return {tail, false};
  }
  bool waitForPredecessor(Node* node, Node* pred, Clock::time_point deadline)
  {
    if(pred == nullptr)
    {
      sMyNode = node;
      return true;
    }
    do
    {
      State predState = pred->mState.load();
      if(predState == State::Released)
      {
        sMyNode = node;
        pred->mState.store(State::Free);
        return true;
      }
      else if(predState == State::Aborted)
      {
        auto predPred = pred->mPred.load();
        pred->mState.store(State::Free);
        pred = predPred;
      }
    }
    while(Clock::now() < deadline);
    node->mPred.store(pred);
    node->mState.store(State::Aborted);
    return false;
  }
  template <typename Rep, typename Period>
  bool tryLock(std::chrono::duration<Rep, Period> dur)
  {
    auto deadline = Clock::now() + dur;
    auto node = getNode(deadline);
    if(!node)
    {
      return false;
    }
    auto [pred, res] = spliceNode(node, deadline);
    if(!res)
    {
      return false;
    }
    return waitForPredecessor(node, pred, deadline);
  }
  void unlock()
  {
    sMyNode->mPred.store(nullptr);
    sMyNode->mState.store(State::Released);
    sMyNode = nullptr;
  }
};

