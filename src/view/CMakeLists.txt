cmake_minimum_required(VERSION 3.6.3)

project(view LANGUAGES C)
kinesis_video_library_setup(${PROJECT_NAME})
target_link_libraries(view heap utils)
kinesis_video_library_install()
