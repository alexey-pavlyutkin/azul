cmake_minimum_required( VERSION 3.15 FATAL_ERROR )

project( jb LANGUAGES CXX )

set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG> )

set_property( GLOBAL PROPERTY USE_FOLDERS ON )

option( AZUL_BUILD_REGRESSION "Build regression tests" ON )
option( AZUL_BUILD_STRESS     "Build stress tests"     ON )

add_subdirectory( include )

if ( AZUL_BUILD_REGRESSION )
    enable_testing()
    find_package( GtestEx )
endif()

if ( AZUL_BUILD_REGRESSION OR AZUL_BUILD_STRESS )
    enable_testing()
    find_package( GtestEx )
    add_subdirectory( test )
endif()

