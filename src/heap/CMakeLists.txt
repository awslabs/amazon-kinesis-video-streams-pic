cmake_minimum_required(VERSION 3.6.3)

project(heap)
kinesis_video_library_setup(${PROJECT_NAME})
target_link_libraries(heap utils)
kinesis_video_library_install()