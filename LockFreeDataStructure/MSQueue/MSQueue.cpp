#include "MSQueue.h"

HazardPointerDomain::HazardPointerList HazardPointerDomain::sHazardPointerList;
HazardPointerDomain::GlobalDeleteList HazardPointerDomain::sGlobalDeleteList;
thread_local HazardPointerDomain::LocalDeleteList HazardPointerDomain::sLocalDeleteList;
thread_local HazardPointerDomain::HazardPointerOwner HazardPointerDomain::sHazardPointerOwner;

std::atomic<void*>& HazardPointerDomain::getHazardPointerForCurrentThread()
{
  return sHazardPointerOwner.getPointer();
}
void HazardPointerDomain::tryDeallocateLocalList()
{
  // we do not have to collect nodes in the global list right away.
  if(sGlobalDeleteList.loadHead(std::memory_order_relaxed))
  {
    auto globalHead = sGlobalDeleteList.resetHead();
    if(globalHead)
    {
      auto cur = globalHead;
      while(cur)
      {
        auto next = cur->mNext;
        sLocalDeleteList.append(cur);
        cur = next;
      }
    }
  }
  auto arr = sHazardPointerList.getPointers();
  auto cur = sLocalDeleteList.resetHead();
  while(cur)
  {
    auto next = cur->mNext;
    if(std::find(arr.begin(), arr.end(), cur->mData) == arr.end())
    {
      delete cur;
    }
    else
    {
      sLocalDeleteList.append(cur);
    }
    cur = next;
  }
}

