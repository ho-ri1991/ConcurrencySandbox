CMAKE_MINIMUM_REQUIRED(VERSION 3.8 FATAL_ERROR)

ADD_EXECUTABLE(test_msqueue
  TestMSQueue.cpp
)

TARGET_LINK_LIBRARIES(test_msqueue
  boost_unit_test_framework
  pthread
  MSQueue
)

ADD_TEST(
  NAME TestMSQueue
  COMMAND $<TARGET_FILE:test_msqueue> --log_level=message
)

ADD_EXECUTABLE(test_lockfreestack
  TestLockFreeStack.cpp
)

TARGET_LINK_LIBRARIES(test_lockfreestack
  boost_unit_test_framework
  pthread
  Stack
)

ADD_TEST(
  NAME TestLockFreeStack
  COMMAND $<TARGET_FILE:test_lockfreestack> --log_level=message
)

ADD_EXECUTABLE(test_lockfreehashmap
  TestLockFreeHashMap.cpp
)

TARGET_LINK_LIBRARIES(test_lockfreehashmap
  boost_unit_test_framework
  pthread
  HashMap
)

ADD_TEST(
  NAME TestLockFreeHashMap
  COMMAND $<TARGET_FILE:test_lockfreehashmap> --log_level=message
)

