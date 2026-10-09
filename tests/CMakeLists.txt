include(GoogleTest)

# add_gtest(<name> <sources>...): a GoogleTest executable whose tests ctest runs.
# Then link what it tests: target_link_libraries(<name> PRIVATE <lib>).
function(add_gtest name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest_main)
    gtest_discover_tests(${name})
endfunction()

add_gtest(app_core_test config_test.cpp logging_test.cpp)
target_link_libraries(app_core_test PRIVATE app_core)

add_gtest(panomax_test panomax_test.cpp)
target_link_libraries(panomax_test PRIVATE panomax)
target_compile_definitions(panomax_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_gtest(ui_test ui_test.cpp)
target_link_libraries(ui_test PRIVATE ui)
