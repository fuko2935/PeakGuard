cmake_minimum_required(VERSION 3.20)
project(PeakGuard LANGUAGES CXX)

add_subdirectory(tools/EndpointProbe)
add_subdirectory(tools/PeakGuardAudioTests)
add_subdirectory(tools/PeakGuardStateProbe)
add_subdirectory(tools/PeakGuardTray)
