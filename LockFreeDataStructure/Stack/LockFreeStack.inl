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
  auto node = mHead.load(std::memory_order_seq_cst);
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
  while(!mHead.compare_exchange_weak(node->mNext, node, std::memory_order_seq_cst, std::memory_order_relaxed));
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
      oldHead = mHead.load(std::memory_order_seq_cst);
      hazardPointer.store(oldHead);
      temp = mHead.load(std::memory_order_seq_cst);
    }
    while(oldHead != temp);
  }
  while(oldHead && !mHead.compare_exchange_strong(oldHead, oldHead->mNext, std::memory_order_seq_cst, std::memory_order_relaxed));
  hazardPointer.store(nullptr, std::memory_order_seq_cst);
  
  std::shared_ptr<T> ans;
  if(oldHead)
  {
    using std::swap;
    swap(ans, oldHead->mData);
    HazardPointerDomain::retire(oldHead, &LockFreeStack::deleteNode);
  }
  return ans;
}

