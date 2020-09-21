#include "CompositeQueueLock.hpp"

thread_local CompositeQueueLock::Node* CompositeQueueLock::sMyNode = nullptr;
