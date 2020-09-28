#pragma once

#include <atomic>
#include <array>
#include <memory>
#include <thread>
#include <functional>
#include <iostream>
#include "HazardPointer.hpp"

template <typename T>
class LockFreeStack
{
private:
  struct Node
  {
    std::shared_ptr<T> mData;
    Node* mNext;
    Node(const T& val): mData(std::make_shared<T>(val)), mNext(nullptr) {}
  };
  std::atomic<Node*> mHead; // operation on this variable have to be memory_order_seq_cst to use hazard pointer
  static void deleteNode(void* node);
public:
  LockFreeStack();
  ~LockFreeStack();
  void push(const T& val);
  std::shared_ptr<T> pop();
};

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
  auto& hazardPointer = HazardPointerDomain<>::getHazardPointerForCurrentThread();
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
    HazardPointerDomain<>::retire(oldHead, &LockFreeStack::deleteNode);
  }
  return ans;
}


