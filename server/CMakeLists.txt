set(EXECUTABLE udp_serial_bridge_server)

add_executable(${EXECUTABLE} main.cpp)
target_compile_features(${EXECUTABLE} PRIVATE cxx_std_17)

target_link_libraries(${EXECUTABLE} PRIVATE osc_serial_bridge_lib)
target_link_libraries(${EXECUTABLE} PUBLIC PkgConfig::LIBUSBP)