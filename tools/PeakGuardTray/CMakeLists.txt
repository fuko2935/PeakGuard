cmake_minimum_required(VERSION 3.20)
project(PeakGuardTray LANGUAGES CXX)

add_executable(PeakGuardTray WIN32 main.cpp LoopbackAudioEngine.cpp PeakGuardTray.rc)
target_compile_features(PeakGuardTray PRIVATE cxx_std_17)
target_compile_definitions(PeakGuardTray PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(PeakGuardTray PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(PeakGuardTray PRIVATE
    comctl32 ole32 oleaut32 uuid propsys advapi32 dwmapi
    mmdevapi avrt)
