cmake_minimum_required( VERSION 3.15 FATAL_ERROR )

project( lfmr LANGUAGES CXX )

set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG> )

set_property( GLOBAL PROPERTY USE_FOLDERS ON )

option( LFMR_BUILD_REGRESSION "Build regression tests" ON )
option( LFMR_BUILD_FUZZING "Build fuzzing tests" ON )
option( LFMR_BUILD_STRESS     "Build stress tests"     OFF )

add_subdirectory( include )

if ( LFMR_BUILD_REGRESSION )
    enable_testing()
    find_package( GtestEx )
endif()

if ( LFMR_BUILD_REGRESSION OR LFMR_BUILD_FUZZING OR LFMR_BUILD_STRESS )
    add_subdirectory( test )
endif()
