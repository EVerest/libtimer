find_package(GTest REQUIRED)
set(TEST_TARGET_NAME ${PROJECT_NAME}_tests)
add_executable(${TEST_TARGET_NAME} libtimer_unit_test.cpp)

target_include_directories(${TEST_TARGET_NAME} PUBLIC${GTEST_INCLUDE_DIRS})

target_link_libraries(${TEST_TARGET_NAME} PRIVATE
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
)

add_test(${TEST_TARGET_NAME} ${TEST_TARGET_NAME})
