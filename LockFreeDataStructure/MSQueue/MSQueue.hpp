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
      tail = mTail.load(std::memory_order_seq_cst);
      hp.store(tail);
      tmp = mTail.load(std::memory_order_seq_cst);
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
  std::unique_ptr<Node> newTail;
  while(true)
  {
    Node* head = nullptr;
    Node* tmp = nullptr;
    while(true)
    {
      head = mHead.load(std::memory_order_seq_cst);
      hp.store(head);
      auto tail = mTail.load(std::memory_order_seq_cst);
      if(head == tail)
      {
        hp.store(tail);
        tmp = mTail.load(std::memory_order_seq_cst);
        if(tail != tmp)
        {
          hp.release();
          continue;
        }
        if(!tail->mData.load())
        {
          hp.release();
          return nullptr;
        }
        auto next = tail->mNext.load();
        if(!next)
        {
          if(!newTail)
          {
            newTail = std::make_unique<Node>();
          }
          Node* expected = nullptr;
          if(tail->mNext.compare_exchange_strong(expected, newTail.get()))
          {
            next = newTail.get();
            newTail.release();
          }
          else
          {
            next = tail->mNext.load();
          }
        }
        mTail.compare_exchange_strong(tail, next);
        hp.release();
        continue;
      }
      tmp = mHead.load(std::memory_order_seq_cst);
      if(head == tmp)
      {
        break;
      }
    }
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

