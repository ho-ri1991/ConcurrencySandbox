#pragma once

#include <queue>
#include <future>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <deque>
#include <type_traits>

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

class ThreadPool
{
private:
  std::atomic<bool> mDone;
  GlobalWorkQueue mGlobalWorkQueue;
  std::vector<std::unique_ptr<LocalWorkQueue>> mLocalTasks;
  std::vector<jthread> mWorkerThreads; // this member variable have to be after LocalQueues because the threads have to be destructed before LocalQueues are destructed.
  static thread_local int sIndex;
  static thread_local LocalWorkQueue* sLocalWorkQueue;
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
        mLocalTasks.push_back(std::make_unique<LocalWorkQueue>());
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

