cmake_minimum_required(VERSION 3.6.3)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
include(Utilities)
project(pic_project_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

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

# the root CMakeLists uses add_definitions if ALIGNED_MEMORY_MODEL or FIXUP_ANNEX_B_TRAILING_NALU_ZERO is set
# https://cmake.org/cmake/help/latest/command/add_definitions.html we build the test project by calling add_directory(tst)
# hence those definitions will be propagated to the test project as well to match the pic build, hence a rebuild
# of pic inside of the test project is not necessary

add_executable(kvspic_test ${PIC_TEST_SOURCE_FILES})
target_link_libraries(kvspic_test kvspic ${GTEST_LIBNAME} ${CMAKE_DL_LIBS} Threads::Threads)
if(UNIX AND NOT APPLE)
    # rt needed for clock_gettime
    target_link_libraries(kvspic_test rt)
endif()

add_test(${PROJECT_NAME} ${PROJECT_NAME})