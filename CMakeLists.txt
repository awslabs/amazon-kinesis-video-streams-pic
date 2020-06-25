cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
include(Utilities)
project(pic_project)

# User Flags
option(BUILD_DEPENDENCIES "Whether or not to build depending libraries from source" ON)

# Developer Flags
option(BUILD_TEST "Build the testing tree." OFF)
option(CODE_COVERAGE "Enable coverage reporting" OFF)
option(COMPILER_WARNINGS "Enable all compiler warnings." OFF)
option(ADDRESS_SANITIZER "Build with AddressSanitizer." OFF)
option(MEMORY_SANITIZER "Build with MemorySanitizer." OFF)
option(THREAD_SANITIZER "Build with ThreadSanitizer." OFF)
option(UNDEFINED_BEHAVIOR_SANITIZER "Build with UndefinedBehaviorSanitizer." OFF)
option(BUILD_DEBUG_HEAP "Build debug heap with guard bands and validation." OFF)
option(ALIGNED_MEMORY_MODEL "Aligned memory model ONLY." OFF)
option(FIXUP_ANNEX_B_TRAILING_NALU_ZERO "Fix-up some bad encoder behavior leaving a trailing zero at the end of NALu" OFF)

if(CODE_COVERAGE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

function(enableSanitizer SANITIZER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls" PARENT_SCOPE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZER}" PARENT_SCOPE)
endfunction()

if(ADDRESS_SANITIZER)
  enableSanitizer("address")
endif()
if(MEMORY_SANITIZER)
  enableSanitizer("memory")
endif()
if(THREAD_SANITIZER)
  enableSanitizer("thread")
endif()
if(UNDEFINED_BEHAVIOR_SANITIZER)
  enableSanitizer("undefined")
endif()
if(BUILD_DEBUG_HEAP)
  add_definitions(-DHEAP_DEBUG)
endif()
if(ALIGNED_MEMORY_MODEL)
  add_definitions(-DALIGNED_MEMORY_MODEL)
endif()
if(FIXUP_ANNEX_B_TRAILING_NALU_ZERO)
  add_definitions(-DFIXUP_ANNEX_B_TRAILING_NALU_ZERO)
endif()

if(BUILD_DEPENDENCIES)
  if (NOT OPEN_SRC_INSTALL_PREFIX)
    set(OPEN_SRC_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/open-source)
    if(NOT EXISTS ${OPEN_SRC_INSTALL_PREFIX})
      file(MAKE_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX})
    endif()
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OPEN_SRC_INSTALL_PREFIX}/lib/pkgconfig")
  endif()

  if(BUILD_TEST)
    build_dependency(gtest)
  endif()
endif()

set(KINESIS_VIDEO_PIC_SRC ${CMAKE_CURRENT_SOURCE_DIR})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
endif()

file(GLOB PIC_CLIENT_SOURCE_FILES "src/client/src/*.c")

file(GLOB PIC_DURATION_SOURCE_FILES "src/duration/src/*.c")

file(GLOB PIC_HEAP_SOURCE_FILES "src/heap/src/*.c")

file(GLOB PIC_MKVGEN_SOURCE_FILES "src/mkvgen/src/*.c")

file(GLOB PIC_STATE_SOURCE_FILES "src/state/src/*.c")

file(GLOB PIC_TRACE_SOURCE_FILES "src/trace/src/*.c")

file(GLOB PIC_UTILS_SOURCE_FILES "src/utils/src/*.c")

file(GLOB PIC_VIEW_SOURCE_FILES "src/view/src/*.c")

file(GLOB PIC_TEST_SOURCE_FILES "src/*/tst/*.cpp")

file(GLOB PIC_HEADERS "${KINESIS_VIDEO_PIC_SRC}/src/*/include")

include_directories(${PIC_HEADERS})
include_directories(${KINESIS_VIDEO_PIC_SRC})

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/libkvspic.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/libkvspic.pc" @ONLY)

add_library(
  kvspic STATIC
  ${PIC_CLIENT_SOURCE_FILES}
  ${PIC_DURATION_SOURCE_FILES}
  ${PIC_HEAP_SOURCE_FILES}
  ${PIC_MKVGEN_SOURCE_FILES}
  ${PIC_STATE_SOURCE_FILES}
  ${PIC_TRACE_SOURCE_FILES}
  ${PIC_UTILS_SOURCE_FILES}
  ${PIC_VIEW_SOURCE_FILES})
target_link_libraries(kvspic ${CMAKE_DL_LIBS} Threads::Threads)
if(UNIX AND NOT APPLE)
  # rt needed for clock_gettime
  target_link_libraries(kvspic rt)
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/libkvspicClient.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/libkvspicClient.pc" @ONLY)

add_library(kvspicClient STATIC ${PIC_CLIENT_SOURCE_FILES})
target_link_libraries(kvspicClient)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/libkvspicState.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/libkvspicState.pc" @ONLY)

add_library(kvspicState STATIC ${PIC_STATE_SOURCE_FILES})
target_link_libraries(kvspicState)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/libkvspicUtils.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/libkvspicUtils.pc" @ONLY)

add_library(kvspicUtils STATIC ${PIC_UTILS_SOURCE_FILES})
target_link_libraries(kvspicUtils ${CMAKE_DL_LIBS} Threads::Threads)
if(UNIX AND NOT APPLE)
  # rt needed for clock_gettime
  target_link_libraries(kvspicUtils rt)
endif()

install(
  TARGETS kvspic kvspicClient kvspicState kvspicUtils
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(
  DIRECTORY ${PIC_HEADERS}
  DESTINATION .)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libkvspic.pc
    ${CMAKE_CURRENT_BINARY_DIR}/libkvspicClient.pc
    ${CMAKE_CURRENT_BINARY_DIR}/libkvspicState.pc
    ${CMAKE_CURRENT_BINARY_DIR}/libkvspicUtils.pc
  DESTINATION lib/pkgconfig)

if(BUILD_TEST)
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

  add_executable(kvspic_test ${PIC_TEST_SOURCE_FILES}
          ${PIC_CLIENT_SOURCE_FILES}
          ${PIC_DURATION_SOURCE_FILES}
          ${PIC_HEAP_SOURCE_FILES}
          ${PIC_MKVGEN_SOURCE_FILES}
          ${PIC_STATE_SOURCE_FILES}
          ${PIC_TRACE_SOURCE_FILES}
          ${PIC_UTILS_SOURCE_FILES}
          ${PIC_VIEW_SOURCE_FILES})
  target_compile_definitions(kvspic_test PRIVATE ALIGNED_MEMORY_MODEL=TRUE FIXUP_ANNEX_B_TRAILING_NALU_ZERO=TRUE)
  target_link_libraries(kvspic_test ${GTEST_LIBNAME} ${CMAKE_DL_LIBS} Threads::Threads)
  if(UNIX AND NOT APPLE)
    # rt needed for clock_gettime
    target_link_libraries(kvspic_test rt)
  endif()
endif()
