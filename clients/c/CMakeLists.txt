cmake_minimum_required(VERSION 2.6)

project(libgibsonclient)

OPTION( WITH_DEBUG "enable debug module" OFF )

# cmake needed modules
include(CheckIncludeFiles)

# configure variables
set( PREFIX usr )
set( PROJECT "libgibsonclient" )
set( AUTHOR "Simone Margaritelli <evilsocket at gmail dot com>" )
set( LICENSE "BSD License" )
set( MAJOR_VERSION 1 )
set( MINOR_VERSION 0 )
set( PATCH_LEVEL   0 )
set( VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_LEVEL}b" )

# common compilation flags
if (WITH_DEBUG)
	message(STATUS "Configuring for debug")
	set( CMAKE_BUILD_TYPE Debug )
	set( OPTIMIZATION "-g -pg" )
else (WITH_DEBUG)
	message(STATUS "Configuring for release")
	set( CMAKE_BUILD_TYPE Release )
	set( OPTIMIZATION "-g -O3" )
endif (WITH_DEBUG)

set( COMMON_CFLAGS "-Wall -Wno-unused-function -std=gnu99 -pedantic ${OPTIMIZATION}" )

file( GLOB_RECURSE MAIN_SOURCES src/*.c )
file( GLOB_RECURSE HEADERS src/*.h )

# generation
add_library( ${PROJECT} SHARED ${MAIN_SOURCES} )
set_target_properties( ${PROJECT} PROPERTIES 
					   # create *nix style library versions + symbolic links
					   VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_LEVEL}" SOVERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_LEVEL}"
					   # Remove the "lib" prefix
					   PREFIX ""
					   # Compile flags
					   COMPILE_FLAGS ${COMMON_CFLAGS}
					   # Install name directory (for Mac OS X install_name_tool)
					   INSTALL_NAME_DIR /${PREFIX}/lib )

install( FILES ${HEADERS} DESTINATION /${PREFIX}/include/ )

install( TARGETS ${PROJECT} 
		 DESTINATION /${PREFIX}/lib 
		 PERMISSIONS
		 OWNER_READ 
		 OWNER_WRITE 
		 OWNER_EXECUTE
		 GROUP_READ 
		 GROUP_EXECUTE 
		 WORLD_READ 
		 WORLD_EXECUTE )

