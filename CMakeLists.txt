cmake_minimum_required(VERSION 2.6)

project(gibson)

OPTION( WITH_DEBUG "enable debug module" OFF )

# cmake needed modules
include_directories("${PROJECT_SOURCE_DIR}/src")
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
set( GIT_COMMAND "git" )

EXECUTE_PROCESS(COMMAND "date" "+%m/%d/%Y %H:%M:%S" OUTPUT_VARIABLE BUILD_DATETIME OUTPUT_STRIP_TRAILING_WHITESPACE)
EXECUTE_PROCESS(COMMAND ${GIT_COMMAND} rev-parse HEAD  OUTPUT_VARIABLE BUILD_GIT_SHA1 OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE SHA1EXIT)
EXECUTE_PROCESS(COMMAND ${GIT_COMMAND} name-rev HEAD   OUTPUT_VARIABLE BUILD_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE BRANCHEXIT)

if(SHA1EXIT)
    set( BUILD_GIT_SHA1 "undefined" )
endif()

if(BRANCHEXIT)
    set( BUILD_GIT_BRANCH "undefined" )
endif()

string(REGEX REPLACE "HEAD " "" BUILD_GIT_BRANCH "${BUILD_GIT_BRANCH}")

if (PREFIX)
    message(STATUS "Using prefix ${PREFIX}")
else (WITH_DEBUG)
	message(STATUS "Using default prefix")
    set( PREFIX /usr )
endif ()


set( PROJECT "gibson" )
set( AUTHOR "Simone Margaritelli <http://gibson-db.in>" )
set( LICENSE "BSD License" )
set( MAJOR_VERSION 1 )
set( MINOR_VERSION 0 )
set( PATCH_LEVEL   0 )
set( VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_LEVEL}b" )

# *BSD
include_directories(/usr/local/include)
# NetBSD
include_directories(/usr/local/include)
# Darwin
include_directories(/opt/local/include)
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

install( TARGETS ${PROJECT} DESTINATION ${PREFIX}/bin )
install( FILES debian/etc/${PROJECT}/${PROJECT}.conf DESTINATION ${PREFIX}/etc/${PROJECT}/ )
install( FILES debian/etc/init.d/${PROJECT} DESTINATION ${PREFIX}/etc/init.d/ 
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

#packaging
if (UNIX)

    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set( CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64" )
    else( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set( CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386" )
    endif( CMAKE_SIZEOF_VOID_P EQUAL 8 )

    set( CPACK_GENERATOR "DEB;RPM")
    set( CPACK_PACKAGE_NAME "gibson-server" )
    set( CPACK_PACKAGE_VERSION  ${VERSION} )
    set( CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}" ) 
    set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "A high performance tree-based cache server.")
    set( CPACK_PACKAGE_DESCRIPTION "A high performance tree-based cache server.")
    set( CPACK_DEBIAN_PACKAGE_SECTION "database" )
    set( CPACK_PACKAGE_CONTACT "Simone Margaritelli evilsocket@gmail.com")

    include(CPack)

endif (UNIX)
