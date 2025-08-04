cmake_minimum_required(VERSION 3.27)
set(CMAKE_CXX_STANDARD 20)
project(NamedPipeLib)
message(STATUS "PRO_DIR ${PROJECT_SOURCE_DIR}")
message(STATUS "CMAKE_DIR ${CMAKE_SOURCE_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
message(STATUS "ARCHIVE OUTPUT DIR = ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
add_library(NamedPipeLib STATIC
        src/win_pipe_server.cpp
        include/win_pipe_server.h
        src/win_pipe_connector.cpp
        include/win_pipe_connector.h
        src/message_dispatcher.cpp
        include/message_dispatcher.h
        src/json_util.cpp
        include/json_util.h
        third/json.hpp
)
target_include_directories(NamedPipeLib PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/third
)
target_link_libraries(NamedPipeLib PRIVATE spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)