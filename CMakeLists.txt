cmake_minimum_required(VERSION 3.19)

project(gradientspace_io)

#use C++20
set (CMAKE_CXX_STANDARD 20)

#todo support this
#option(GSIO_USE_STATIC_LIBRARY "use GradientspaceIO as a static library")

# what are these for?
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# find all files in top-level Private and Public source folders
file(GLOB_RECURSE PRIVATE_FILES DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.*")
file(GLOB_RECURSE PUBLIC_FILES DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.*")
set(MODULE_FILES ${PRIVATE_FILES} ${PUBLIC_FILES})

add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../GradientspaceCore ${CMAKE_CURRENT_BINARY_DIR}/gscore_bin )

# configure the library
add_library(gradientspace_io SHARED ${MODULE_FILES})

# maintain directory tree in generated project files
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR})

# set up the GRADIENTSPACEIO_API macro...
target_compile_definitions(gradientspace_io PRIVATE GRADIENTSPACEIO_EXPORTS)

# set include directories
target_include_directories(gradientspace_io PUBLIC "Public")
target_include_directories(gradientspace_io PUBLIC "Public/MeshIO")
target_include_directories(gradientspace_io PRIVATE "Private")

# add dependencies
target_link_libraries(gradientspace_io PUBLIC gradientspace_core)
