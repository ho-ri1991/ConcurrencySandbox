#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

class InterruptFlag
{
private:
  std::atomic<bool> mFlag;
  std::condition_variable* mCond;
  std::mutex mSetClearCondLock;
public:
  InterruptFlag();
  InterruptFlag(const InterruptFlag&) = delete;
  InterruptFlag& operator=(const InterruptFlag&) = delete;
  ~InterruptFlag() = default;
  void set();
  bool isSet() const;
  void setConditionVariable(std::condition_variable& cond);
  void clearConditionVariable();
};

class InterruptException: public std::exception
{
public:
  const char* what() const noexcept override;
};

template <typename Pred>
void interruptibleWait(std::unique_lock<std::mutex>& lock, std::condition_variable& cond, Pred pred);
void interruptibleWait(std::unique_lock<std::mutex>& lock, std::condition_variable& cond);
void interruptionPoint();

class InterruptibleThread
{
  template <typename Pred>
  friend void interruptibleWait(std::unique_lock<std::mutex>& lock, std::condition_variable& cond, Pred pred);
  friend void interruptibleWait(std::unique_lock<std::mutex>& lock, std::condition_variable& cond);
  friend void interruptionPoint();
private:
  std::thread mThread;
  std::atomic<InterruptFlag*> mInterruptFlag;
  static thread_local InterruptFlag sInterruptFlag;
public:
  template <typename F>
  explicit InterruptibleThread(F&& f);
  InterruptibleThread(InterruptibleThread&&) = default;
  InterruptibleThread& operator=(InterruptibleThread&&) = default;
  ~InterruptibleThread() = default;
  void join();
  void detach();
  bool joinable() const;
  void interrupt();
};

#include "InterruptibleThread.inl"

