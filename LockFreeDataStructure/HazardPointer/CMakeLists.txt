CMAKE_MINIMUM_REQUIRED(VERSION 3.8 FATAL_ERROR)

ADD_LIBRARY(HazardPointer INTERFACE)

TARGET_INCLUDE_DIRECTORIES(HazardPointer
  INTERFACE
  ../HazardPointer
)

