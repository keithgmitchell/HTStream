cmake_minimum_required(VERSION 3.2)
project(gtest_builder C CXX)
include(ExternalProject)

ExternalProject_Add(googletest
    URL https://github.com/google/googletest/archive/release-1.10.0.tar.gz
    CMAKE_ARGS -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG:PATH=DebugLibs
               -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE:PATH=ReleaseLibs
               -DCMAKE_CXX_FLAGS=${MSVC_COMPILER_DEFS}
               -Dgtest_force_shared_crt=ON
               -DBUILD_GTEST=ON
     PREFIX "${CMAKE_CURRENT_BINARY_DIR}"
     URL_MD5 "ecd1fa65e7de707cd5c00bdac56022cd"
# Disable install step
    INSTALL_COMMAND ""
)

# Specify include dir
ExternalProject_Get_Property(googletest source_dir)
set(GTEST_INCLUDE_DIRS ${source_dir}/googletest/include CACHE PATH "" FORCE)

# Specify MainTest's link libraries
ExternalProject_Get_Property(googletest binary_dir)
set(GTEST_LIBS_DIR ${binary_dir}/lib CACHE PATH "" FORCE)
message( status "gtest libdir: " ${GTEST_LIBS_DIR})
