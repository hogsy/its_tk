cmake_minimum_required(VERSION 3.9)
project(its_extract C)

# Set all of our output directories.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

set(CMAKE_C_STANDARD 11)

add_executable(its_extract main.c)