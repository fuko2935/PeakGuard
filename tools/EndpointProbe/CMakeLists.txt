cmake_minimum_required(VERSION 3.20)
project(EndpointProbe LANGUAGES CXX)

add_executable(EndpointProbe main.cpp)
target_compile_features(EndpointProbe PRIVATE cxx_std_17)
target_compile_definitions(EndpointProbe PRIVATE UNICODE _UNICODE)
target_link_libraries(EndpointProbe PRIVATE ole32 uuid propsys)
