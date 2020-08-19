template <typename Deleter>
void HazardPointerDomain::appendToLocalDeleteList(void* data, Deleter deleter)
{
  sLocalDeleteList.append(data, deleter);
}
template <typename Deleter>
void HazardPointerDomain::retire(void* data, Deleter deleter) 
{
  appendToLocalDeleteList(data, deleter);
  if(2 * sHazardPointerList.size() < sLocalDeleteList.size())
  {
    tryDeallocateLocalList();
  }
}

