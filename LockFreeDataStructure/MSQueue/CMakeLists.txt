CMAKE_MINIMUM_REQUIRED(VERSION 3.8 FATAL_ERROR)

ADD_LIBRARY(MSQueue INTERFACE)

TARGET_LINK_LIBRARIES(MSQueue
  INTERFACE
  HazardPointer
)

TARGET_INCLUDE_DIRECTORIES(MSQueue
  INTERFACE
  ../MSQueue
)

