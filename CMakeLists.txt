cmake_minimum_required(VERSION 2.6)

project(gibson)

OPTION( WITH_DEBUG "enable debug module" OFF )

# cmake needed modules
include(CheckIncludeFiles)
include(CheckLibraryExists)

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

set( COMMON_FLAGS "-Wall ${OPTIMIZATION}" )

set(HAVE_JEMALLOC 0)

if (WITH_JEMALLOC)
	FIND_LIBRARY(JEMALLOC_LIB jemalloc)
	if (JEMALLOC_LIB)
		CHECK_LIBRARY_EXISTS(jemalloc mallctl "" HAVE_JEMALLOC_FUN)
		if (HAVE_JEMALLOC_FUN)
			message(STATUS "Using jemalloc memory allocator at ${JEMALLOC_LIB}")
			set(HAVE_JEMALLOC 1)
		else()
			message(STATUS "Found jemalloc at ${JEMALLOC_LIB}, but unable to find its API "
			               "(maybe the library was configured with a non-empty function prefix?)")
		endif()
	else() 
		message(STATUS "Can't find jemalloc")
	endif()
else (WITH_JEMALLOC)
	message(STATUS "Using standard libc memory allocator." )
endif (WITH_JEMALLOC)

# configure variables
EXECUTE_PROCESS(COMMAND "date" "+%m/%d/%Y %H:%M:%S" OUTPUT_VARIABLE BUILD_DATETIME OUTPUT_STRIP_TRAILING_WHITESPACE)
EXECUTE_PROCESS(COMMAND "git" rev-parse HEAD OUTPUT_VARIABLE BUILD_GIT_SHA1 OUTPUT_STRIP_TRAILING_WHITESPACE)
EXECUTE_PROCESS(COMMAND "git" name-rev HEAD OUTPUT_VARIABLE BUILD_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX REPLACE "HEAD " "" BUILD_GIT_BRANCH "${BUILD_GIT_BRANCH}")

set( PREFIX usr )
set( PROJECT "gibson" )
set( AUTHOR "Simone Margaritelli <evilsocket at gmail dot com>" )
set( LICENSE "BSD License" )
set( MAJOR_VERSION 1 )
set( MINOR_VERSION 0 )
set( PATCH_LEVEL   0 )
set( VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_LEVEL}b" )

# this is needed for Mac OS X compilation compatibility
include_directories("${PROJECT_SOURCE_DIR}/src")

file( GLOB MAIN_SOURCES src/*.c )
file( GLOB HEADERS src/*.h )

# configure.h generation
configure_file( src/configure.h.in src/configure.h )

# generation
add_executable( ${PROJECT} ${MAIN_SOURCES} ) 
set_target_properties( ${PROJECT} PROPERTIES COMPILE_FLAGS "${COMMON_CFLAGS}" )

if ( HAVE_JEMALLOC EQUAL 1 )
	target_link_libraries( ${PROJECT} jemalloc )
endif ( HAVE_JEMALLOC EQUAL 1 )

install( TARGETS ${PROJECT} DESTINATION /${PREFIX}/bin )
install( FILES debian/etc/${PROJECT}/${PROJECT}.conf DESTINATION /etc/${PROJECT}/ )
install( FILES debian/etc/init.d/${PROJECT} DESTINATION /etc/init.d/ 
		 PERMISSIONS
		 OWNER_READ 
		 OWNER_WRITE 
		 OWNER_EXECUTE
		 GROUP_READ 
		 GROUP_EXECUTE 
		 WORLD_READ 
		 WORLD_EXECUTE
)

# tests
find_program( PHP_FOUND NAMES php )	
if ( NOT PHP_FOUND )
	message( STATUS "-- Could not find php, tests disabled")
else ( NOT PHP_FOUND )
	message( "-- Found php : ${PHP_FOUND}\n-- Run 'make test' to execute testsuite" )
	
	add_custom_target( test COMMAND ./tests/run_tests.php )
endif ( NOT PHP_FOUND )
