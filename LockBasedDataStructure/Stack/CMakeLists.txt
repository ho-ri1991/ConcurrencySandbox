CMAKE_MINIMUM_REQUIRED(VERSION 3.8)

PROJECT(ThreadSafeStack)

SET(CMAKE_EXPORT_COMPILE_COMMANDS YES)

ENABLE_TESTING()
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

#https://blog.kitware.com/cmake-and-the-default-build-type/
SET(DEFAULT_BUILD_TYPE "Debug")
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  MESSAGE(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}'")
  SET(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
  SET_PROPERTY(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")

FIND_PACKAGE(Boost 1.62.0 REQUIRED COMPONENTS unit_test_framework)
INCLUDE_DIRECTORIES(AFTER SYSTEM ${Boost_INCLUDE_DIR})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

ADD_EXECUTABLE(test_threadsafestack
  TestThreadSafeStack.cpp
)

TARGET_LINK_LIBRARIES(test_threadsafestack
  boost_unit_test_framework
)

ADD_TEST(
  NAME TestThreadSafeStack
  COMMAND $<TARGET_FILE:test_threadsafestack> --log_level=message
)

