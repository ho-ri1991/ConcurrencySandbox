template <typename Deleter>
void HazardPointerDomain::appendToLocalDeleteList(void* data, Deleter deleter)
{
  sLocalDeleteList.append(data, deleter);
}
template <typename Deleter>
void HazardPointerDomain::retire(void* data, Deleter deleter) 
{
  if(isHazardous(data))
  {
    appendToLocalDeleteList(data, deleter);
  }
  else
  {
    deleter(data);
  }
}

template <typename T>
void LockFreeStack<T>::deleteNode(void* node)
{
  delete reinterpret_cast<Node*>(node);
}
template <typename T>
LockFreeStack<T>::LockFreeStack(): mHead(nullptr) {}
template <typename T>
LockFreeStack<T>::~LockFreeStack()
{
  auto node = mHead.load();
  while(node)
  {
    auto next = node->mNext;
    delete node;
    node = next;
  }
}
template <typename T>
void LockFreeStack<T>::push(const T& val)
{
  auto node = new Node(val);
  node->mNext = mHead.load();
  while(!mHead.compare_exchange_weak(node->mNext, node));
}
template <typename T>
std::shared_ptr<T> LockFreeStack<T>::pop()
{
  auto& hazardPointer = HazardPointerDomain::getHazardPointerForCurrentThread();
  Node* oldHead;
  do
  {
    Node* temp;
    do
    {
      oldHead = mHead.load();
      hazardPointer.store(oldHead);
      temp = mHead.load();
    }
    while(oldHead != temp);
  }
  while(oldHead && !mHead.compare_exchange_strong(oldHead, oldHead->mNext));
  hazardPointer.store(nullptr);
  
  std::shared_ptr<T> ans;
  if(oldHead)
  {
    using std::swap;
    swap(ans, oldHead->mData);
    HazardPointerDomain::retire(oldHead, &LockFreeStack::deleteNode);
    HazardPointerDomain::tryDeallocateLocalList();
  }
  return ans;
}
