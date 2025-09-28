# Tests CMakeLists.txt
option(BUILD_SLICK_LOGGER_TESTING "Build tests" ON)

if(BUILD_SLICK_LOGGER_TESTING)
    find_package(GTest QUIET)

    if(GTest_FOUND)
        add_executable(slick_logger_tests test_logger.cpp)
        target_include_directories(slick_logger_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(slick_logger_tests slick_logger GTest::gtest_main)
        add_test(NAME slick_logger_tests COMMAND slick_logger_tests)

        add_executable(slick_logger_sink_tests test_sinks.cpp)
        target_include_directories(slick_logger_sink_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(slick_logger_sink_tests slick_logger GTest::gtest_main)
        add_test(NAME slick_logger_sink_tests COMMAND slick_logger_sink_tests)

        add_executable(slick_logger_timestamp_tests test_timestamp.cpp)
        target_include_directories(slick_logger_timestamp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(slick_logger_timestamp_tests slick_logger GTest::gtest_main)
        add_test(NAME slick_logger_timestamp_tests COMMAND slick_logger_timestamp_tests)
        
        message(STATUS "GTest found - building slick_logger tests")
    else()
        message(STATUS "GTest not found - skipping tests")
    endif()
else()
    message(STATUS "Testing disabled - skipping slick_logger tests")
endif()
