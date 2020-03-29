cmake_minimum_required(VERSION 3.9)

if(DEFINED PROJECT_NAME)
  set(SUBPROJECT ON)
endif()

project(
  actorpp
  VERSION 0.0.1
  LANGUAGES CXX)

add_subdirectory(submodules)

find_package(Threads REQUIRED)

add_library(actorpp INTERFACE)
add_library(actorpp::actorpp ALIAS actorpp)
target_include_directories(actorpp INTERFACE ${PROJECT_SOURCE_DIR}/include)

target_compile_features(actorpp INTERFACE cxx_std_11)
target_link_libraries(actorpp INTERFACE Threads::Threads)

if(NOT SUBPROJECT)
  include(CTest)
  add_subdirectory(test)
endif()
