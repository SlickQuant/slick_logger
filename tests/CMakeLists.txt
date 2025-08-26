# Tests CMakeLists.txt
find_package(GTest QUIET)

if(GTest_FOUND)
    add_executable(slick_logger_tests test_logger.cpp)
    target_link_libraries(slick_logger_tests slick_logger GTest::gtest_main)
    add_test(NAME slick_logger_tests COMMAND slick_logger_tests)

    add_executable(slick_logger_sink_tests test_sinks.cpp)
    target_link_libraries(slick_logger_sink_tests slick_logger GTest::gtest_main)
    add_test(NAME slick_logger_sink_tests COMMAND slick_logger_sink_tests)

    add_executable(slick_logger_timestamp_tests test_timestamp.cpp)
    target_link_libraries(slick_logger_timestamp_tests slick_logger GTest::gtest_main)
    add_test(NAME slick_logger_timestamp_tests COMMAND slick_logger_timestamp_tests)
    
    message(STATUS "GTest found - building tests")
else()
    message(STATUS "GTest not found - skipping tests")
endif()
