#include "HierarchicalMutex.hpp"
#include <limits>

void HierarchicalMutex::updateCurrentHierarchy()
{
  mPreviousHierarchy = sThisThreadHierarchy;
  sThisThreadHierarchy = mHierarchy;
}
void HierarchicalMutex::checkHierarchy()
{
  if(sThisThreadHierarchy <= mHierarchy)
  {
    throw std::runtime_error("hierarchy violation");
  }
}
HierarchicalMutex::HierarchicalMutex(HierarchyType hierarchy): mHierarchy(hierarchy) {}
void HierarchicalMutex::lock()
{
  checkHierarchy();
  mLock.lock();
  updateCurrentHierarchy();
}
void HierarchicalMutex::unlock()
{
  if(sThisThreadHierarchy != mHierarchy)
  {
    throw std::runtime_error("hierarchy violation");
  }
  sThisThreadHierarchy = mPreviousHierarchy;
  mLock.unlock();
}
bool HierarchicalMutex::try_lock()
{
  checkHierarchy();
  if(!mLock.try_lock())
  {
    return false;
  }
  updateCurrentHierarchy();
  return true;
}
thread_local HierarchicalMutex::HierarchyType HierarchicalMutex::sThisThreadHierarchy(std::numeric_limits<HierarchicalMutex::HierarchyType>::max());

