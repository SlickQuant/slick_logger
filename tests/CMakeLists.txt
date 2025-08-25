# Tests CMakeLists.txt
find_package(GTest REQUIRED)
add_executable(slick_logger_tests test_logger.cpp)
target_link_libraries(slick_logger_tests slick_logger GTest::gtest_main)
add_test(NAME slick_logger_tests COMMAND slick_logger_tests)
