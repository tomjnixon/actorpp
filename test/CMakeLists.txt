include(${PROJECT_SOURCE_DIR}/submodules/catch2.cmake)

function(add_actorpp_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE actorpp catch2)
  add_test(
    NAME ${name}
    COMMAND $<TARGET_FILE:${name}>
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/test")
endfunction()

add_actorpp_test(basic_tests basic_tests.cpp)

add_actorpp_test(net_tests_shutdown net_tests.cpp)
target_compile_definitions(net_tests_shutdown
                           PRIVATE ACTORPP_RECV_THREAD_SHUTDOWN)

add_actorpp_test(net_tests_pipe net_tests.cpp)
target_compile_definitions(net_tests_pipe PRIVATE ACTORPP_RECV_THREAD_PIPE)
