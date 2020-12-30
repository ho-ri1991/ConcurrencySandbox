#pragma once

#include <queue>
#include <future>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <deque>
#include <type_traits>
#include <cassert>
#include "HazardPointer.hpp"

class jthread
{
private:
  std::thread t;
public:
  template <typename F, typename... Args>
  jthread(F&& f, Args&&... args): t(std::forward<F>(f), std::forward<Args>(args)...)
  {
    static_assert(std::is_invocable_v<F, Args&&...>);
  }
  jthread(const jthread&) = delete;
  jthread(jthread&&) = default;
  jthread& operator=(const jthread) = delete;
  jthread& operator=(jthread&&) = default;
  void join()
  {
    t.join();
  }
  ~jthread() noexcept
  {
    if(t.joinable())
    {
      t.join();
    }
  }
};

class Task
{
private:
  class TaskHolder
  {
  public:
    virtual void invoke() = 0;
    virtual ~TaskHolder() = default;
  };
  template <typename Fn>
  class TaskHolderImpl: public TaskHolder
  {
  private:
    Fn fn;
  public:
    TaskHolderImpl(Fn&& fn): fn(std::forward<Fn>(fn)) {}
    void invoke() override { fn(); }
  };
  std::unique_ptr<TaskHolder> fn;
public:
  Task() = default;
  template <typename Fn>
  Task(Fn&& fn): fn(std::make_unique<TaskHolderImpl<std::decay_t<Fn>>>(std::forward<Fn>(fn)))
  {
  }
  Task(Task&&) = default;
  Task& operator=(Task&&) = default;
  void operator()() { fn->invoke(); }
  explicit operator bool () { return fn.get() != nullptr; }
};

class GlobalWorkQueue
{
private:
  std::queue<Task> mTasks;
  mutable std::mutex mLock;
public:
  GlobalWorkQueue() = default;
  GlobalWorkQueue(const GlobalWorkQueue&) = delete;
  GlobalWorkQueue& operator=(const GlobalWorkQueue&) = delete;
  ~GlobalWorkQueue() = default;
  void push(Task&& task)
  {
    std::lock_guard lk(mLock);
    mTasks.push(std::move(task));
  }
  bool pop(Task& task)
  {
    std::lock_guard lk(mLock);
    if(mTasks.empty())
    {
      return false;
    }
    task = std::move(mTasks.front());
    mTasks.pop();
    return true;
  }
};

class LocalWorkQueue
{
private:
  std::deque<Task> mTasks;
  mutable std::mutex mLock;
public:
  LocalWorkQueue() = default;
  LocalWorkQueue(const LocalWorkQueue&) = delete;
  LocalWorkQueue& operator=(const LocalWorkQueue&) = delete;
  ~LocalWorkQueue() = default;
  void push(Task&& task)
  {
    std::lock_guard lk(mLock);
    mTasks.push_front(std::move(task));
  }
  bool pop(Task& task)
  {
    std::lock_guard lk(mLock);
    if(mTasks.empty())
    {
      return false;
    }
    task = std::move(mTasks.front());
    mTasks.pop_front();
    return true;
  }
  bool steal(Task& task)
  {
    std::lock_guard lk(mLock);
    if(mTasks.empty())
    {
      return false;
    }
    task = std::move(mTasks.back());
    mTasks.pop_back();
    return true;
  }
};

class LockFreeLocalWorkQueue
{
private:
  class CircularArray
  {
  private:
    std::vector<std::atomic<Task*>> mTasks;
  public:
    CircularArray(std::size_t capacity): mTasks(capacity) {}
    ~CircularArray()
    {
      // TODO: make CircularArray exception safe
      //       memory leaks will not occur if all the tasks are done by worker threads
//      for(auto& t: mTasks)
//      {
//        Task* p = t.load();
//        if (p)
//        {
//          delete p;
//        }
//      }
    }
    std::size_t capacity() const noexcept { return mTasks.size(); }
    Task* get(std::size_t i) { return mTasks[i % mTasks.size()]; }
    void put(std::size_t i, Task task) { mTasks[i % mTasks.size()] = new Task(std::move(task)); }
    void put(std::size_t i, Task* task) { mTasks[i % mTasks.size()] = task; }
    std::unique_ptr<CircularArray> resize(std::size_t bottom, std::size_t top)
    {
      auto newTasks = std::make_unique<CircularArray>(mTasks.size() * 2);
      for(std::size_t i = top; i < bottom; ++i)
      {
        newTasks->put(i, get(i));
      }
      return newTasks;
    }
    static void deleter(void* p) { delete reinterpret_cast<CircularArray*>(p); }
  };
  std::atomic<CircularArray*> mTasks;
  std::atomic<long long> mBottom;
  std::atomic<long long> mTop;
public:
  LockFreeLocalWorkQueue(std::size_t initialCapacity = 1 << 3)
    : mTasks(new CircularArray(initialCapacity))
    , mBottom(0)
    , mTop(0) {}
  ~LockFreeLocalWorkQueue() { delete mTasks.exchange(nullptr, std::memory_order_relaxed); }
  void push(Task&& task)
  {
    auto b = mBottom.load();
    auto t = mTop.load();
    auto size = b - t;
    auto tasks = mTasks.load();
    if (tasks->capacity() - 1 <= static_cast<std::size_t>(size))
    {
      auto newTasks = mTasks.load()->resize(b, t);
      // safe to use store instead of exchange because push is called by only one specific thread
      mTasks.store(newTasks.get());
      HazardPointerDomain<>::retire(tasks, &CircularArray::deleter);
      tasks = newTasks.release();
    }
    tasks->put(b, std::move(task));
    // safe to use store instead of fetch_add because push and pop are called by one specific thread
    mBottom.store(b + 1);
  }
  bool pop(Task& task)
  {
    auto oldBottom = mBottom.load();
    auto newBottom = oldBottom - 1;
    mBottom.store(newBottom);
    auto oldTop = mTop.load();
    auto size = newBottom - oldTop;
    if (size < 0)
    {
      mBottom.store(oldTop);
      return false;
    }
    auto hpHolder = HazardPointerHolder(HazardPointerDomain<>::getHazardPointerForCurrentThread());
    auto tasks = claimPointer(mTasks, hpHolder);
    auto p = tasks->get(newBottom);
    if (0 < size)
    {
      task = std::move(*p);
      delete p;
      assert(task);
      return true;
    }
    bool success = false;
    auto expected = oldTop;
    if (mTop.compare_exchange_strong(expected, oldTop + 1))
    {
      task = std::move(*p);
      delete p;
      assert(task);
      success = true;
    }
    mBottom.store(oldTop + 1);
    return success;
  }
  bool steal(Task& task)
  {
    auto oldTop = mTop.load();
    auto bottom = mBottom.load();
    auto size = bottom - oldTop;
    if (size <= 0)
    {
      return false;
    }
    HazardPointerHolder hpHolder(HazardPointerDomain<>::getHazardPointerForCurrentThread());
    auto tasks = claimPointer(mTasks, hpHolder);
    auto p = tasks->get(oldTop);
    if (mTop.compare_exchange_strong(oldTop, oldTop + 1))
    {
      task = std::move(*p);
      delete p;
      assert(task);
      return true;
    }
    return false;
  }
};

template <typename LocalWorkQueueType = LocalWorkQueue>
class ThreadPool
{
private:
  std::atomic<bool> mDone;
  GlobalWorkQueue mGlobalWorkQueue;
  std::vector<std::unique_ptr<LocalWorkQueueType>> mLocalTasks;
  std::vector<jthread> mWorkerThreads; // this member variable have to be after LocalQueues because the threads have to be destructed before LocalQueues are destructed.
  static thread_local int sIndex;
  static thread_local LocalWorkQueueType* sLocalWorkQueue;
private:
  void work(int i)
  {
    sIndex = i;
    sLocalWorkQueue = mLocalTasks[i].get();
    while(!mDone)
    {
      runPendingTask();
    }
  }
  bool getFromLocalQueue(Task& task)
  {
    return sLocalWorkQueue ? sLocalWorkQueue->pop(task): false;
  }
  bool getFromGlobalQueue(Task& task)
  {
    return mGlobalWorkQueue.pop(task);
  }
  bool getFromOtherLocalQueue(Task& task)
  {
    for(std::size_t i = 0; i < mLocalTasks.size(); ++i)
    {
      auto& queue = *mLocalTasks[(sIndex + i + 1) % mLocalTasks.size()];
      if(queue.steal(task))
      {
        return true;
      }
    }
    return false;
  }
public:
  ThreadPool(std::size_t numThread = std::thread::hardware_concurrency()):
    mDone(false)
  {
    try
    {
      // construct LocalQueues before starting worker threads to avoid data race in mLocalTasks
      for(std::size_t i = 0; i < numThread; ++i)
      {
        mLocalTasks.push_back(std::make_unique<LocalWorkQueueType>());
      }
      for(std::size_t i = 0; i < numThread; ++i)
      {
        mWorkerThreads.emplace_back(&ThreadPool::work, this, i);
      }
    }
    catch(...)
    {
      mDone.store(true);
      throw;
    }
  }
  ~ThreadPool()
  {
    mDone.store(true);
  }
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  template <typename Fn>
  auto submit(Fn fn) -> std::enable_if_t<std::is_invocable_v<Fn>, std::future<std::invoke_result_t<Fn>>>
  {
    using result_type = std::invoke_result_t<Fn>;
    std::packaged_task<result_type()> task(fn);
    auto res = task.get_future();
    if(sLocalWorkQueue)
    {
      sLocalWorkQueue->push(std::move(task));
    }
    else
    {
      mGlobalWorkQueue.push(std::move(task));
    }
    return res;
  }
  void runPendingTask()
  {
    Task task;
    if(getFromLocalQueue(task) || getFromGlobalQueue(task) || getFromOtherLocalQueue(task))
    {
      task();
    }
    else
    {
      std::this_thread::yield();
    }
  }
};

template <typename LocalWorkQueueType>
thread_local int ThreadPool<LocalWorkQueueType>::sIndex = 0;
template <typename LocalWorkQueueType>
thread_local LocalWorkQueueType* ThreadPool<LocalWorkQueueType>::sLocalWorkQueue = nullptr;
