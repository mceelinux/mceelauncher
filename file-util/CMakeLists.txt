cmake_minimum_required (VERSION 3.5)

set(CMAKE_CXX_STANDARD 11)

project(file-util LANGUAGES CXX)

add_library(file-util include/FileUtil.h src/FileUtil.cpp include/EnvPathUtil.h src/EnvPathUtil.cpp)
if (APPLE)
    target_sources(file-util PRIVATE src/EnvPathUtil_MacOS.mm)
endif()
target_include_directories(file-util PUBLIC include/)
if (APPLE)
    target_link_libraries(file-util "-framework Foundation")
endif()
if (TARGET logger)
    target_link_libraries(file-util logger)
    target_compile_definitions(file-util PRIVATE HAVE_LOGGER)
endif()
