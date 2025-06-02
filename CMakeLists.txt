cmake_minimum_required(VERSION 3.9)

if(DEFINED PROJECT_NAME)
  set(SUBPROJECT ON)
endif()

project(
  actorpp
  VERSION 0.0.1
  LANGUAGES CXX)

include(GNUInstallDirs)

find_package(Threads REQUIRED)

add_library(actorpp INTERFACE)
add_library(actorpp::actorpp ALIAS actorpp)
target_include_directories(actorpp INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_compile_features(actorpp INTERFACE cxx_std_11)
target_link_libraries(actorpp INTERFACE Threads::Threads)

if(NOT SUBPROJECT)
  include(CTest)

  if(FIND_CATCH2)
    find_package(Catch2 3 REQUIRED)
  else()
    add_subdirectory(submodules/catch2)
  endif()

  add_subdirectory(test)
endif()

install(TARGETS actorpp
  EXPORT actorpp_targets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/actorpp TYPE INCLUDE)
install(EXPORT actorpp_targets
  FILE actorpp_targets.cmake
  NAMESPACE actorpp::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/actorpp
)
