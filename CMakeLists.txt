cmake_minimum_required(VERSION 3.20)

project(EdgeNetworkGuard VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(EdgeNetworkGuard WIN32
    src/main.cpp
    src/app_config.cpp
    src/logger.cpp
    src/network_probe.cpp
    src/http_client.cpp
    src/edge_controller.cpp
    src/task_scheduler_installer.cpp
    src/tray_app.cpp
    src/app.rc
)

target_include_directories(EdgeNetworkGuard PRIVATE src)

target_compile_definitions(EdgeNetworkGuard PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(EdgeNetworkGuard PRIVATE
    icmp
    iphlpapi
    ole32
    oleaut32
    taskschd
    winhttp
    ws2_32
)
