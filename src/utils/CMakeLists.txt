cmake_minimum_required(VERSION 3.6.3)

project(utils LANGUAGES C)
kinesis_video_library_setup(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})
kinesis_video_library_install()

option(KVS_DEFAULT_STACK_SIZE "Set default stack size in bytes" "")
option(CONSTRAINED_DEVICE "Enable constrained device mode" OFF)

# Ensure variables are available as non-cache variables
if (DEFINED KVS_DEFAULT_STACK_SIZE)
    set(KVS_DEFAULT_STACK_SIZE ${KVS_DEFAULT_STACK_SIZE} CACHE STRING "Stack size in bytes" FORCE)
endif()

if (DEFINED CONSTRAINED_DEVICE)
    set(CONSTRAINED_DEVICE ${CONSTRAINED_DEVICE} CACHE BOOL "Flag for constrained devices" FORCE)
endif()
