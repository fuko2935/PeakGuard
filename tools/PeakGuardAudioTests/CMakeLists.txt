cmake_minimum_required(VERSION 3.20)
project(PeakGuardAudioTests LANGUAGES CXX)

add_executable(PeakGuardAudioTests main.cpp)
target_compile_features(PeakGuardAudioTests PRIVATE cxx_std_17)
target_compile_definitions(PeakGuardAudioTests PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(PeakGuardAudioTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(PeakGuardAudioTests PRIVATE ole32)
