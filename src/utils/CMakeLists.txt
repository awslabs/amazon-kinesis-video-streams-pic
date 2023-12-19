cmake_minimum_required(VERSION 3.6.3)

project(utils LANGUAGES C)
kinesis_video_library_setup(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})
kinesis_video_library_install()
