cmake_minimum_required(VERSION 3.0.0)
project(osc_serial_bridge VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(CTest)
enable_testing()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSBP REQUIRED IMPORTED_TARGET libusbp-1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
add_executable(osc_udp_serial_bridge Bridge.cpp SLIP.cpp main.cpp)

target_link_libraries(osc_udp_serial_bridge PUBLIC PkgConfig::LIBUSBP)


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
