#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

template <typename ClusterTraits>
class HierarchicalCLHQueueLock
{
private:
  static constexpr std::size_t sClusterSize = ClusterTraits::ClusterSize;
  static constexpr std::size_t hardware_destructive_interference_size = 64;
  struct Node
  {
  private:
    static constexpr std::uint32_t sClusterMask = 0x3FFFFFFF;
    static constexpr std::uint32_t sSuccessorMustWaitMask = 0x40000000;
    static constexpr std::uint32_t sTailWhenSplicedMask = 0x80000000;
    std::atomic<std::uint32_t> mState;
  public:
    struct ThreadLocalVariableInitialize {};
    static constexpr ThreadLocalVariableInitialize sThreadLocalInitialize{};
    Node(): mState(0) {}
    Node(ThreadLocalVariableInitialize): mState((ClusterTraits::getClusterID() | sSuccessorMustWaitMask) & ~sTailWhenSplicedMask) {}
    void clear()
    {
      auto oldState = mState.load();
      std::uint32_t newState = ClusterTraits::getClusterID();
      newState |= sSuccessorMustWaitMask;
      newState &= ~sTailWhenSplicedMask;
      while(!mState.compare_exchange_weak(oldState, newState));
    }
    bool getSuccessorMustWait() const
    {
      return mState.load() & sSuccessorMustWaitMask;
    }
    void clearSuccessorMustWait()
    {
      auto oldState = mState.load();
      std::uint32_t newState;
      do
      {
        newState = oldState & ~sSuccessorMustWaitMask;
      }
      while(!mState.compare_exchange_strong(oldState, newState));
    }
    bool waitForGrantOrClusterMaster() const
    {
      auto thisClusterID = ClusterTraits::getClusterID();
      while(true)
      {
        auto state = mState.load();
        auto clusterID = state & sClusterMask;
        bool successorMustWait = state & sSuccessorMustWaitMask;
        bool tailWhenSpliced = state & sTailWhenSplicedMask;
        if(clusterID == thisClusterID && !successorMustWait && !tailWhenSpliced)
        {
          return true;
        }
        else if(clusterID != thisClusterID || tailWhenSpliced)
        {
          return false;
        }
      }
    }
    void setTailWhenSpliced()
    {
      auto oldState = mState.load();
      std::uint32_t newState;
      do
      {
        newState = oldState | sTailWhenSplicedMask;
      }
      while(!mState.compare_exchange_weak(oldState, newState));
    }
  };
  struct alignas(hardware_destructive_interference_size) AlignedNodePointer
  {
    std::atomic<Node*> mPointer;
  };
  AlignedNodePointer mLocalTails[sClusterSize];
  AlignedNodePointer mGlobalTail;
  class GarbageCollector
  {
  private:
    struct ListNode
    {
      Node* mNode;
      ListNode* mNext;
      ListNode(Node* node): mNode(node), mNext(nullptr) {}
      ~ListNode()
      {
        delete mNode;
      }
    };
    std::atomic<ListNode*> mTail;
  public:
    GarbageCollector(): mTail(nullptr) {}
    ~GarbageCollector()
    {
      auto tail = mTail.exchange(nullptr);
      while(tail)
      {
        auto next = tail->mNext;
        delete tail;
        tail = next; 
      }
    }
    void append(Node* node)
    {
      auto tail = new ListNode(node);
      tail->mNext = mTail.load();
      while(!mTail.compare_exchange_weak(tail->mNext, tail));
    }
  };
  class ThreadLocalNodeHolder
  {
  private:
    Node* mNode;
  public:
    ThreadLocalNodeHolder(GarbageCollector& garbageCollector): mNode(new Node(Node::sThreadLocalInitialize))
    {
      garbageCollector.append(mNode);
    }
    Node* operator->() { return mNode; }
    const Node* operator->() const { return mNode; }
    Node& operator*() { return *mNode; }
    const Node& operator*() const { return *mNode; }
    ThreadLocalNodeHolder& operator=(Node* node) { mNode = node; }
    Node* get() { return mNode; }
    Node* reset(Node* node) { auto tmp = mNode; mNode = node; return tmp; }
    Node* release() { auto tmp = mNode; mNode = nullptr; return tmp; }
  };
  static GarbageCollector& getGarbageCollector()
  {
    static GarbageCollector gc;
    return gc;
  }
  static thread_local ThreadLocalNodeHolder sMyNode;
  static thread_local Node* sMyPred;
public:
  HierarchicalCLHQueueLock()
  {
    auto node = new Node();
    mGlobalTail.mPointer.store(node);
    getGarbageCollector().append(node);
    for(auto& p: mLocalTails)
    {
      p.mPointer.store(nullptr);
    }
  }
  void lock()
  {
    auto& localTail = mLocalTails[ClusterTraits::getClusterID()].mPointer;
    auto pred1 = localTail.exchange(sMyNode.get());
    if(pred1)
    {
      auto isOwnLock = pred1->waitForGrantOrClusterMaster();
      if(isOwnLock)
      {
        sMyPred = pred1;
        return;
      }
    }
    // the thread is a cluster master
    Node* tail;
    auto pred = mGlobalTail.mPointer.load();
    do
    {
      tail = localTail.load();
    }
    while(!mGlobalTail.mPointer.compare_exchange_strong(pred, tail));
    tail->setTailWhenSpliced();
    while(pred->getSuccessorMustWait());
    sMyPred = pred;
    return;
  }
  void unlock()
  {
    auto oldMyNode = sMyNode.release();
    sMyPred->clear();
    sMyNode.reset(sMyPred);
    sMyPred = nullptr;
    oldMyNode->clearSuccessorMustWait();
  }
};

template <typename ClusterTraits>
thread_local typename HierarchicalCLHQueueLock<ClusterTraits>::ThreadLocalNodeHolder HierarchicalCLHQueueLock<ClusterTraits>::sMyNode(getGarbageCollector());
template <typename ClusterTraits>
thread_local typename HierarchicalCLHQueueLock<ClusterTraits>::Node* HierarchicalCLHQueueLock<ClusterTraits>::sMyPred = nullptr;

