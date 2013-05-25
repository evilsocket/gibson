cmake_minimum_required(VERSION 2.6)

project(gibson)

OPTION( WITH_DEBUG "enable debug module" OFF )

# cmake needed modules
include(CheckIncludeFiles)

# configure variables
set( PREFIX usr )
set( PROJECT "gibson" )
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
	set( OPTIMIZATION "-g -O2" )
endif (WITH_DEBUG)

set( COMMON_CFLAGS "-Wall -std=gnu99 -pedantic ${OPTIMIZATION}" )

file( GLOB_RECURSE MAIN_SOURCES src/*.c )
file( GLOB_RECURSE HEADERS src/*.h )

# configure.h generation
configure_file( src/configure.h.in src/configure.h )

# generation
add_executable( ${PROJECT} ${MAIN_SOURCES} ) 
set_target_properties( ${PROJECT} PROPERTIES COMPILE_FLAGS "${COMMON_CFLAGS}" )

install( TARGETS ${PROJECT} DESTINATION /${PREFIX}/sbin )
install( FILES ${PROJECT}.conf DESTINATION /etc/gibson )
install( FILES init.d/${PROJECT} DESTINATION /etc/init.d/ 
		 PERMISSIONS
		 OWNER_READ 
		 OWNER_WRITE 
		 OWNER_EXECUTE
		 GROUP_READ 
		 GROUP_EXECUTE 
		 WORLD_READ 
		 WORLD_EXECUTE
)
