cmake_minimum_required(VERSION 3.20)
project(LimiterAudioTests LANGUAGES CXX)

add_executable(LimiterAudioTests main.cpp)
target_compile_features(LimiterAudioTests PRIVATE cxx_std_17)
target_compile_definitions(LimiterAudioTests PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(LimiterAudioTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(LimiterAudioTests PRIVATE ole32)
