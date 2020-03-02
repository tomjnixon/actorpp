include(${PROJECT_SOURCE_DIR}/submodules/catch2.cmake)

function(add_actorpp_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE actorpp catch2)
  add_test(
    NAME ${name}
    COMMAND $<TARGET_FILE:${name}>
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/test")
endfunction()

add_actorpp_test(basic_tests)
add_actorpp_test(net_tests)
