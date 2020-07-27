#include "HierarchicalMutex.hpp"

HierarchicalMutex m1(1);
HierarchicalMutex m2(2);
HierarchicalMutex m3(3);

int main()
{
  {
    std::lock_guard l3(m3);
    std::lock_guard l2(m2);
    std::lock_guard l1(m1);
  }
  {
    try
    {
      std::lock_guard l2(m2);
      std::lock_guard l3(m3);
    }
    catch(std::exception& ex)
    {
    }
  }
}

