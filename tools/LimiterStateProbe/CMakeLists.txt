cmake_minimum_required(VERSION 3.20)
project(LimiterStateProbe LANGUAGES CXX)

add_executable(LimiterStateProbe main.cpp)
target_compile_features(LimiterStateProbe PRIVATE cxx_std_17)
target_compile_definitions(LimiterStateProbe PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(LimiterStateProbe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(LimiterStateProbe PRIVATE advapi32)
