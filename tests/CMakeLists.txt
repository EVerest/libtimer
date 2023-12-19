find_package(GTest REQUIRED)
add_executable(libtimer_unit_test libtimer_unit_test.cpp)

target_include_directories(libtimer_unit_test PUBLIC${GTEST_INCLUDE_DIRS})

target_link_libraries(libtimer_unit_test PRIVATE
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
)

add_test(libtimer_unit_test libtimer_unit_test)
