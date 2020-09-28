#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <iostream>
#include "HazardPointer.hpp"

template<typename T>
class MSQueue
{
private:
  struct Node
  {
    std::atomic<T*> mData;
    std::atomic<Node*> mNext;
    Node(): mData(nullptr), mNext(nullptr) {}
  };
  std::atomic<Node*> mHead;
  std::atomic<Node*> mTail;
  static void deleteNode(void* node);
public:
  MSQueue();
  ~MSQueue();
  MSQueue(const MSQueue&) = delete;
  MSQueue(MSQueue&&) = delete;
  MSQueue& operator=(const MSQueue&) = delete;
  MSQueue& operator=(MSQueue&&) = delete;
  void push(const T& data);
  std::unique_ptr<T> tryPop();
};

template <typename T>
void MSQueue<T>::deleteNode(void* node)
{
  delete reinterpret_cast<Node*>(node);
}

template <typename T>
MSQueue<T>::MSQueue(): mHead(new Node()), mTail(mHead.load()) {}

template <typename T>
MSQueue<T>::~MSQueue()
{
  auto node = mHead.load();
  while(node)
  {
    auto next = node->mNext.load();
    delete node->mData.load();
    delete node;
    node = next;
  }
}

template <typename T>
void MSQueue<T>::push(const T& data)
{
  HazardPointerHolder hp(HazardPointerDomain<>::getHazardPointerForCurrentThread());
  auto p = std::make_unique<T>(data);
  auto node = std::make_unique<Node>();
  while(true)
  {
    Node* tail = nullptr;
    Node* tmp = nullptr;
    do
    {
      tail = mTail.load();
      hp.store(tail);
      tmp = mTail.load();
    }
    while(tail != tmp);
    T* expected = nullptr;
    if(tail->mData.compare_exchange_strong(expected, p.get()))
    {
      p.release();
      Node* expected = nullptr;
      Node* newNext = node.get();
      if(tail->mNext.compare_exchange_strong(expected, node.get()))
      {
        node.release();
      }
      else
      {
        newNext = expected;
      }
      mTail.compare_exchange_strong(tail, newNext);
      break;
    }
    else
    {
      Node* expected = nullptr;
      Node* newNext = node.get();
      if(tail->mNext.compare_exchange_strong(expected, node.get()))
      {
        node.release();
        node = std::make_unique<Node>();
      }
      else
      {
        newNext = expected;
      }
      mTail.compare_exchange_strong(tail, newNext);
    }
  }
}

template <typename T>
std::unique_ptr<T> MSQueue<T>::tryPop()
{
  HazardPointerHolder hp(HazardPointerDomain<>::getHazardPointerForCurrentThread());
  while(true)
  {
    Node* head = nullptr;
    Node* tmp = nullptr;
    do
    {
      head = mHead.load();
      hp.store(head);
      if(head == mTail.load())
      {
        return nullptr;
      }
      tmp = mHead.load();
    }
    while(head != tmp);
    auto next = head->mNext.load();
    if(mHead.compare_exchange_strong(head, next))
    {
      std::unique_ptr<T> ans(head->mData);
      hp.release();
      HazardPointerDomain<>::retire(head, &deleteNode);
      return ans;
    }
  }
}

