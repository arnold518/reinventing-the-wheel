cmake_minimum_required(VERSION 3.10)
project(CircuitSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include headers
include_directories(include)

# Collect all cpp files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Create executable
add_executable(sim ${SOURCES})
