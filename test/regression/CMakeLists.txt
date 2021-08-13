cmake_minimum_required( VERSION 3.15 FATAL_ERROR )

#
# add regression executable...
#
add_executable( regression
    accessor.h
    heap.cpp
)


#
# ...and linking dependencies
#
target_link_libraries( regression PRIVATE gtest gtest_main azul )


#
# set PARANOID mode
#
if ( MSVC )
    target_compile_options( regression PRIVATE /W4 /WX /bigobj )
else()
    target_compile_options( regression PRIVATE -Wall -Wextra -Wpedantic -Werror )
endif()


#
# fetch GTest cases
#
gtest_add_tests( TARGET regression AUTO WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG> )


#
# IDE: move targets to "test" folder
#
set_target_properties( regression PROPERTIES FOLDER test )


#
# specify install destination for regression and concurrent_file_opener targets
#
install( TARGETS regression DESTINATION ${CMAKE_BINARY_DIR}/bin/$<CONFIG> )
