if ( LFMR_BUILD_REGRESSION )
    add_subdirectory( regression )
endif()

if ( LFMR_BUILD_FUZZING )
    add_subdirectory( fuzzing )
endif()

if ( LFMR_BUILD_STRESS )
#    add_subdirectory( stress )
endif()
