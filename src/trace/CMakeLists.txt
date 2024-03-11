cmake_minimum_required(VERSION 3.6.3)

project(trace LANGUAGES C)
kinesis_video_library_setup(${PROJECT_NAME})
target_link_libraries(trace utils)
kinesis_video_library_install()
