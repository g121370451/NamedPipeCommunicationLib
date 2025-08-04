cmake_minimum_required(VERSION 3.27)
project(SensorBridgeServer)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/server)
message(STATUS "Server EXE Out Path" = ${EXECUTABLE_OUTPUT_PATH})
message(STATUS "Server Project DIR = ${PROJECT_SOURCE_DIR}")
message(STATUS "Server Cmake DIR = ${CMAKE_SOURCE_DIR}")
add_executable(SensorBridgeServer ${PROJECT_SOURCE_DIR}/main.cpp
)
target_link_libraries(SensorBridgeServer
        PRIVATE
        NamedPipeLib
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)