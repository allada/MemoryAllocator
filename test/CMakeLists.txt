cmake_minimum_required (VERSION 2.8.8)
project (MemoryAllocator_test)

set(CMAKE_CXX_FLAGS "-g -O0 -Wall -std=c++11")

add_subdirectory(${CMAKE_SOURCE_DIR}/gtest)

set(HEADERS ../MemoryAllocator.h)
set(TEST_SRC_FILES test.cpp)

enable_testing()
set(PROJECT_TEST_NAME MemoryAllocator_test)
include_directories(${GTEST_INCLUDE_DIRS} ${BASEPATH})
add_executable(${PROJECT_TEST_NAME} ${HEADERS} ${TEST_SRC_FILES})
add_dependencies(${PROJECT_TEST_NAME} googletest)

target_link_libraries(${PROJECT_TEST_NAME}
    ${GTEST_LIBS_DIR}/libgtest.a
    ${GTEST_LIBS_DIR}/libgtest_main.a
)

target_link_libraries(${PROJECT_TEST_NAME} ${CMAKE_THREAD_LIBS_INIT})
add_test(test1 ${PROJECT_TEST_NAME})