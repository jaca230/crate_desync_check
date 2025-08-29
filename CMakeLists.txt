cmake_minimum_required(VERSION 3.14)
project(unpacker VERSION 1.0 LANGUAGES CXX)

# ----------------------- Compiler Settings ------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-stringop-overflow)
endif()

# ----------------------- Source Files -----------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# ----------------------- Executable -------------------------------
add_executable(${PROJECT_NAME} ${SOURCES})

# ----------------------- MIDAS System -----------------------------
if(NOT DEFINED ENV{MIDASSYS})
    message(FATAL_ERROR "Environment variable MIDASSYS not set. Please point to your MIDAS installation.")
endif()

set(MIDAS_INCLUDE_DIRS
    $ENV{MIDASSYS}/include
    $ENV{MIDASSYS}/midasio
)

set(MIDAS_LIBS
    $ENV{MIDASSYS}/lib/libmfe.a
    $ENV{MIDASSYS}/lib/libmidas.a
)

# ----------------------- Find ZLIB -------------------------------
find_package(ZLIB REQUIRED)

# ----------------------- Include Directories ----------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${MIDAS_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ----------------------- Link Libraries ---------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${MIDAS_LIBS}
    ${ZLIB_LIBRARIES}
)

# ----------------------- Optional Compile Definitions ------------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    -DWD2_DONT_INCLUDE_REG_ACCESS_VARS
    -DDCB_DONT_INCLUDE_REG_ACCESS_VARS
)

# ----------------------- Output Directories ----------------------
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
