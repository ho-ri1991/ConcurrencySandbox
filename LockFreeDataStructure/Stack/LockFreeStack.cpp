#include "LockFreeStack.hpp"

std::array<HazardPointerDomain::PointerWithThreadID, HazardPointerDomain::sArraySize> HazardPointerDomain::sPointerWithThreadID;
HazardPointerDomain::GlobalDeleteList HazardPointerDomain::sGlobalDeleteList;
std::atomic<std::size_t> HazardPointerDomain::sNumHazardPointer(0);
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
      auto cur = globalHead;
      while(cur)
      {
        auto next = cur->mNext;
        sLocalDeleteList.append(cur);
        cur = next;
      }
    }
  }
  std::vector<void*> arr;
  arr.reserve(sNumHazardPointer.load());
  for(auto& p: sPointerWithThreadID)
  {
    arr.push_back(p.mPointer.load());
  }
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

