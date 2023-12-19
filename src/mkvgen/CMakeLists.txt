cmake_minimum_required(VERSION 3.6.3)

project(mkvgen LANGUAGES C)
kinesis_video_library_setup(${PROJECT_NAME})
target_link_libraries(mkvgen utils)
kinesis_video_library_install()
