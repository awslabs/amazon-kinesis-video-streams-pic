cmake_minimum_required(VERSION 3.6.3)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
include(Utilities)
project(pic_project_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

if (OPEN_SRC_INSTALL_PREFIX)
    find_package(GTest REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
else()
    find_package(GTest REQUIRED)
endif()

SET(GTEST_LIBNAME GTest::gtest)
if (TARGET GTest::GTest)
    SET(GTEST_LIBNAME GTest::GTest)
endif()

file(GLOB PIC_TEST_SOURCE_FILES "*/*.cpp")


add_executable(kvspic_test ${PIC_TEST_SOURCE_FILES})
target_compile_definitions(kvspic_test PRIVATE ALIGNED_MEMORY_MODEL=TRUE FIXUP_ANNEX_B_TRAILING_NALU_ZERO=TRUE)
target_link_libraries(kvspic_test kvspic ${GTEST_LIBNAME} ${CMAKE_DL_LIBS} Threads::Threads)
if(UNIX AND NOT APPLE)
    # rt needed for clock_gettime
    target_link_libraries(kvspic_test rt)
endif()