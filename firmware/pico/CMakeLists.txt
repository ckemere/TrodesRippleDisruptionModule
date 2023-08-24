# What CMake to start at
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CSTANDARD 11)
 
# Include the subsidiary .cmake file to get the SDK
include(pico_sdk_import.cmake)
 
# Set the name and version of the project
project(stim-pulser VERSION 1.0.0)
 
# Link the Project to a source file (step 4.6)
add_executable(stim-pulser source.c)
 
# Link the Project to an extra library (pico_stdlib)
target_link_libraries(stim-pulser pico_stdlib)
 
# Initalise the SDK
pico_sdk_init()
 
# Enable USB, UART output
pico_enable_stdio_usb(stim-pulser 0)
pico_enable_stdio_uart(stim-pulser 0)
 
# Enable extra outputs (SWD?)
pico_add_extra_outputs(stim-pulser)
