# Note that headers are optional, and do not affect add_library, but they will not
# show up in IDEs unless they are listed in add_library.

file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${osc_serial_bridge_SOURCE_DIR}/include/*.hpp")

set(OSC_BRIDGE_LIB "osc_serial_bridge_lib")

# Make an automatic library - will be static or dynamic based on user setting
add_library(${OSC_BRIDGE_LIB} Bridge.cpp SLIP.cpp ${HEADER_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(${OSC_BRIDGE_LIB} PUBLIC ../include)

# This depends on (header only) boost
target_link_libraries(${OSC_BRIDGE_LIB} PRIVATE Boost::boost)

# All users of this library will need at least C++11
target_compile_features(${OSC_BRIDGE_LIB} PUBLIC cxx_std_17)

# IDEs should put the headers in a nice place
source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})