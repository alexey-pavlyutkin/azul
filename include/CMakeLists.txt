cmake_minimum_required( VERSION 3.15 )


#
# create INTERFACE library (pure template approach)
#
add_library( lfmr INTERFACE )


#
# make it IDE-friendly incorporating header files
#
file( GLOB_RECURSE header_files lfmr/*.h )
target_sources( lfmr INTERFACE ${header_files} )


#
# add current directory to include search paths
#
target_include_directories( lfmr INTERFACE . )


#
# the library uses C++17
#
target_compile_features( lfmr INTERFACE cxx_std_17 )

