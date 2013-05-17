cmake_minimum_required(VERSION 2.6)

project(gibson)

OPTION( WITH_DEBUG "enable debug module" OFF )

# cmake needed modules
include(CheckIncludeFiles)

# config variables
set( PROJECT "gibson" )
set( AUTHOR "Simone Margaritelli" )

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
	set( OPTIMIZATION "-O2" )
endif (WITH_DEBUG)

set( COMMON_CFLAGS "-w -Wunused ${OPTIMIZATION}" )

file( GLOB_RECURSE MAIN_SOURCES src/*.c )
file( GLOB_RECURSE HEADERS src/*.h )

# generation
add_executable( ${PROJECT} ${MAIN_SOURCES} ) 
set_target_properties( ${PROJECT} PROPERTIES COMPILE_FLAGS "${COMMON_CFLAGS}" )

