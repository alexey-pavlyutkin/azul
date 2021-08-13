cmake_minimum_required( VERSION 3.15 )

#
# create INTERFACE library (pure template approach)
#
add_library( azul INTERFACE )

#
# make it IDE-friendly incorporating header files
#
file( GLOB_RECURSE header_files azul/*.h )
target_sources( azul INTERFACE ${header_files} )


#
# add current directory to include search paths
#
target_include_directories( azul INTERFACE . )

#
# the library uses C++17
#
target_compile_features( azul INTERFACE cxx_std_17 )

