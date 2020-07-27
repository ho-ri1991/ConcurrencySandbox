#pragma once
#include <mutex>

class HierarchicalMutex
{
public:
  using HierarchyType = unsigned long long;
private:
  const HierarchyType mHierarchy;
  HierarchyType mPreviousHierarchy;
  static thread_local HierarchyType sThisThreadHierarchy;
  std::mutex mLock;
private:
  void updateCurrentHierarchy();
  void checkHierarchy();
public:
  HierarchicalMutex(HierarchyType hierarchy);
  void lock();
  void unlock();
  bool try_lock();
};
