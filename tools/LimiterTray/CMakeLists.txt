cmake_minimum_required(VERSION 3.20)
project(LimiterTray LANGUAGES CXX)

add_executable(LimiterTray WIN32 main.cpp LoopbackAudioEngine.cpp LimiterTray.rc)
target_compile_features(LimiterTray PRIVATE cxx_std_17)
target_compile_definitions(LimiterTray PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(LimiterTray PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(LimiterTray PRIVATE
    comctl32 ole32 oleaut32 uuid propsys advapi32 dwmapi
    mmdevapi avrt)
