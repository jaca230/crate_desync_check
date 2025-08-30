cmake_minimum_required(VERSION 3.14)
project(unpacker VERSION 1.0 LANGUAGES CXX)

# ----------------------- Compiler Settings ------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-stringop-overflow)
endif()

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

# ----------------------- Main Executable (Desync Checker) --------
add_executable(desync_checker src/main.cpp)

target_include_directories(desync_checker PRIVATE
    ${MIDAS_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(desync_checker PRIVATE
    ${MIDAS_LIBS}
    ${ZLIB_LIBRARIES}
)

target_compile_definitions(desync_checker PRIVATE
    -DWD2_DONT_INCLUDE_REG_ACCESS_VARS
    -DDCB_DONT_INCLUDE_REG_ACCESS_VARS
)

# ----------------------- Corruption Analyzer Executable ----------
add_executable(analyze_results src/analyze_results.cpp)

target_include_directories(analyze_results PRIVATE
    ${MIDAS_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(analyze_results PRIVATE
    ${MIDAS_LIBS}
    ${ZLIB_LIBRARIES}
)

target_compile_definitions(analyze_results PRIVATE
    -DWD2_DONT_INCLUDE_REG_ACCESS_VARS
    -DDCB_DONT_INCLUDE_REG_ACCESS_VARS
)

# ----------------------- Subrun Analyzer Executable -------------
add_executable(analyze_subruns src/analyze_subruns.cpp)

target_include_directories(analyze_subruns PRIVATE
    ${MIDAS_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(analyze_subruns PRIVATE
    ${MIDAS_LIBS}
    ${ZLIB_LIBRARIES}
)

target_compile_definitions(analyze_subruns PRIVATE
    -DWD2_DONT_INCLUDE_REG_ACCESS_VARS
    -DDCB_DONT_INCLUDE_REG_ACCESS_VARS
)

# ----------------------- Output Directories ----------------------
set_target_properties(desync_checker analyze_results analyze_subruns PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)