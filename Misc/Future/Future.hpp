#include <memory>
#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <type_traits>
#include <variant>
#include <exception>

namespace my
{
template <typename T>
struct SharedState
{
  // TODO: support void and reference
  static_assert(std::is_same_v<T, std::decay_t<T>> && !std::is_same_v<T, void>);
  std::mutex mLock;
  std::condition_variable mCond;
  bool mIsReady = false;
  struct Data
  {
    alignas(T) std::byte mBytes[sizeof(T)];
  };
  std::variant<Data, std::exception_ptr> mData;
};

template <typename R>
class Future
{
  template <typename T>
  friend class Promise;
private:
  std::shared_ptr<SharedState<R>> mSharedState;
  Future(const std::shared_ptr<SharedState<R>>& state);
public:
  Future() = default;
  Future(Future&& other) = default;
  Future(const Future&) = delete;
  Future& operator=(Future&& other) = default;
  Future& operator=(const Future& other) = delete;
  bool valid() const;
  R get();
  void wait();
  void swap(Future& other) noexcept;
};

template <typename R>
Future<R>::Future(const std::shared_ptr<SharedState<R>>& state): mSharedState(state)
{
}
template <typename R>
bool Future<R>::valid() const
{
  return mSharedState != nullptr;
}
template <typename R>
R Future<R>::get()
{
  if(!valid())
  {
    throw std::runtime_error("invalid future");
  }
  std::unique_lock lk(mSharedState->mLock);
  mSharedState->mCond.wait(lk, [this](){ return mSharedState->mIsReady; });
  if(mSharedState->mData.index() == 0)
  {
    return std::move(*std::launder(reinterpret_cast<R*>(std::get<0>(mSharedState->mData).mBytes)));
  }
  else
  {
    std::rethrow_exception(std::get<1>(mSharedState->mData));
  }
}
template <typename R>
void Future<R>::wait()
{
  if(!valid())
  {
    throw std::runtime_error("invalid future");
  }
  std::unique_lock lk(mSharedState->mLock);
  mSharedState->mCond.wait(lk, [this](){ return mSharedState->mIsReady; });
}
template <typename R>
void Future<R>::swap(Future& other) noexcept
{
  using std::swap;
  swap(mSharedState, other.mSharedState);
}

template <typename R>
class Promise
{
private:
  std::shared_ptr<SharedState<R>> mSharedState;
public:
  Promise() = default;
  Promise(Promise&& other) = default;
  Promise(const Promise&) = delete;
  Promise& operator=(Promise&& other) = default;
  Promise& operator=(const Promise& other) = delete;
  Future<R> getFuture();
  void setValue(const R& r);
  void setValue(R&& r);
  void setException(std::exception_ptr p);
  void swap(Promise& other) noexcept;
};

template <typename R>
Future<R> Promise<R>::getFuture()
{
  if(mSharedState)
  {
    throw std::runtime_error("getFuture is called twice");
  }
  mSharedState = std::make_shared<SharedState<R>>();
  return Future<R>(mSharedState);
}
template <typename R>
void Promise<R>::setValue(const R& r)
{
  std::unique_lock lk(mSharedState->mLock);
  new(std::get<0>(mSharedState->mData).mBytes) R(r);
  mSharedState->mIsReady = true;
  mSharedState->mCond.notify_one();
}
template <typename R>
void Promise<R>::setValue(R&& r)
{
  std::unique_lock lk(mSharedState->mLock);
  new(std::get<0>(mSharedState->mData).mBytes) R(std::move(r));
  mSharedState->mIsReady = true;
  mSharedState->mCond.notify_one();
}
template <typename R>
void Promise<R>::setException(std::exception_ptr p)
{
  std::unique_lock lk(mSharedState->mLock);
  mSharedState->mData.template emplace<1>(p);
  mSharedState->mIsReady = true;
  mSharedState->mCond.notify_one();
}
template <typename R>
void Promise<R>::swap(Promise& other) noexcept
{
  using std::swap;
  swap(mSharedState, other.mSharedState);
}

}

