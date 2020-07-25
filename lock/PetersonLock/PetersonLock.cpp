#include "PetersonLock.hpp"

std::atomic<int> PetersonLock::sIDCount{0};
