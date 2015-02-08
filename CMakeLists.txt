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
    set( OPTIMIZATION "-g -O3 -DNDEBUG" )
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

if (PREFIX)
    message(STATUS "Using prefix ${PREFIX}")
else (WITH_DEBUG)
	message(STATUS "Using default prefix")
    set( PREFIX /usr )
endif ()

set( PROJECT "gibson" )
set( AUTHOR "Simone Margaritelli <http://gibson-db.in>" )
set( LICENSE "BSD License" )
set( VERSION "1.0.2" )

set( TESTS_FOLDER "tests" )
set( TESTS_REPO "https://github.com/evilsocket/ruby-gibson.git" )

# this is needed for Mac OS X compilation compatibility
include_directories("${PROJECT_SOURCE_DIR}/src")

file( GLOB MAIN_SOURCES src/*.c )
file( GLOB HEADERS src/*.h )

# configure.h generation
configure_file( src/configure.h.in src/configure.h )
# generation
add_executable( ${PROJECT} ${MAIN_SOURCES} ) 

# backtrace is available in a separate library under FreeBSD
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    message( STATUS "Detected FreeBSD - Using libexecinfo." )
    include_directories(/usr/local/include)
    # cmake seems pretty buggy under FreeBSD :(
    link_directories(/usr/local/lib)
    set( CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib" )
    set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L/usr/local/lib" )
    set( CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -L/usr/local/lib" )

    target_link_libraries( ${PROJECT} execinfo )
endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

set_target_properties( ${PROJECT} PROPERTIES COMPILE_FLAGS "${COMMON_CFLAGS}" )

if ( HAVE_JEMALLOC EQUAL 1 )
	target_link_libraries( ${PROJECT} jemalloc )
endif ( HAVE_JEMALLOC EQUAL 1 )

install( TARGETS ${PROJECT} DESTINATION ${PREFIX}/bin )
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

## tests

# check if ruby is installed
find_program( RUBY_BIN NAMES ruby )

if ( NOT RUBY_BIN )

    message( STATUS "-- Could not find ruby, tests disabled" )

else ( NOT RUBY_BIN )

    message( "-- Found ruby : ${RUBY_BIN}\n-- Run 'make test' to execute testsuite" )

    if ( NOT IS_DIRECTORY "${TESTS_FOLDER}" )
        message( STATUS "-- Folder '${TESTS_FOLDER}' not found, cloning ${TESTS_REPO} ..." )
        
        EXECUTE_PROCESS(COMMAND ${GIT_COMMAND} clone ${TESTS_REPO} ${TESTS_FOLDER})
    endif ( NOT IS_DIRECTORY "${TESTS_FOLDER}" )

    add_custom_target( test COMMAND cd ${TESTS_FOLDER} && rake test )
endif ( NOT RUBY_BIN )

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
