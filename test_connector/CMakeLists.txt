cmake_minimum_required(VERSION 3.27)
project(SensorBridgeConnector)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/connector)
message(STATUS "Server EXE Out Path" = ${EXECUTABLE_OUTPUT_PATH})
message(STATUS "Connector Project DIR = ${PROJECT_SOURCE_DIR}")
message(STATUS "Connector Cmake DIR = ${CMAKE_SOURCE_DIR}")
add_executable(SensorBridgeConnector ${PROJECT_SOURCE_DIR}/main.cpp
)
target_link_libraries(SensorBridgeConnector
        PRIVATE
        NamedPipeLib
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)