cmake_minimum_required(VERSION 3.20)
project(PeakGuardStateProbe LANGUAGES CXX)

add_executable(PeakGuardStateProbe main.cpp)
target_compile_features(PeakGuardStateProbe PRIVATE cxx_std_17)
target_compile_definitions(PeakGuardStateProbe PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(PeakGuardStateProbe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(PeakGuardStateProbe PRIVATE advapi32)
