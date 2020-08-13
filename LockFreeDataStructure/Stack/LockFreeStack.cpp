#include "LockFreeStack.hpp"

std::array<HazardPointerDomain::PointerWithThreadID, HazardPointerDomain::sArraySize> HazardPointerDomain::sPointerWithThreadID;
HazardPointerDomain::GlobalDeleteList HazardPointerDomain::sGlobalDeleteList;
thread_local HazardPointerDomain::LocalDeleteList HazardPointerDomain::sLocalDeleteList;
thread_local HazardPointerDomain::HazardPointerOwner HazardPointerDomain::sHazardPointerOwner;

bool HazardPointerDomain::isHazardous(void* data)
{
  for(auto& p: sPointerWithThreadID)
  {
    if(p.mPointer.load() == data)
    {
      return true;
    }
  }
  return false;
}
std::atomic<void*>& HazardPointerDomain::getHazardPointerForCurrentThread()
{
  return sHazardPointerOwner.getPointer();
}
void HazardPointerDomain::tryDeallocateLocalList()
{
  if(sGlobalDeleteList.loadHead())
  {
    auto globalHead = sGlobalDeleteList.resetHead();
    if(globalHead)
    {
      auto last = globalHead;
      while(last->mNext)
      {
        last = last->mNext;
      }
      last->mNext = sLocalDeleteList.resetHead();
      sLocalDeleteList.append(globalHead);
    }
  }
  auto cur = sLocalDeleteList.resetHead();
  while(cur)
  {
    auto next = cur->mNext;
    if(!isHazardous(cur->mData))
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

