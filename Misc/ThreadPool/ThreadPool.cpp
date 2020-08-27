#include "ThreadPool.hpp"

thread_local int ThreadPool::sIndex = 0;
thread_local LocalWorkQueue* ThreadPool::sLocalWorkQueue = nullptr;

