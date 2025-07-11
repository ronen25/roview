cmake_minimum_required(VERSION 3.20)
project(roview C)

include(FetchContent)
include(ExternalProject)

set(CMAKE_C_STANDARD 17)

FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
)

ExternalProject_Add(
        mupdf_external
        GIT_REPOSITORY https://github.com/ArtifexSoftware/mupdf.git
        GIT_TAG 1.26.3
        GIT_SHALLOW TRUE
        SOURCE_DIR ${CMAKE_BINARY_DIR}/mupdf-src
        BINARY_DIR ${CMAKE_BINARY_DIR}/mupdf-build
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make -C ${CMAKE_BINARY_DIR}/mupdf-src libs
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS
        ${CMAKE_BINARY_DIR}/mupdf-src/build/release/libmupdf.a
        ${CMAKE_BINARY_DIR}/mupdf-src/build/release/libmupdf-third.a
)

FetchContent_MakeAvailable(raylib)

add_library(mupdf STATIC IMPORTED)
add_library(mupdf-third STATIC IMPORTED)

# Add dependencies
add_dependencies(mupdf mupdf_external)
add_dependencies(mupdf-third mupdf_external)

set_target_properties(mupdf PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/mupdf-src/build/release/libmupdf.a
)

set_target_properties(mupdf-third PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/mupdf-src/build/release/libmupdf-third.a
)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_compile_definitions(roview DEBUG)
endif ()

set(SRCS
        main.c
        renderer.c
        pdfpage.c
        log.c
)
add_executable(roview ${SRCS})
target_link_libraries(roview PRIVATE raylib mupdf mupdf-third)
target_include_directories(roview PRIVATE
        ${CMAKE_BINARY_DIR}/mupdf-src/include
)
