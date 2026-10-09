include(GoogleTest)

# add_gtest(<name> <sources>...): a GoogleTest executable whose tests ctest runs.
# Then link what it tests: target_link_libraries(<name> PRIVATE <lib>).
function(add_gtest name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest_main)
    gtest_discover_tests(${name})
endfunction()

add_gtest(app_core_test logging_test.cpp)
target_link_libraries(app_core_test PRIVATE app_core)
