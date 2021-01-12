cmake_minimum_required(VERSION 3.12)

project(nonsense CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 10.0.0)
        message(FATAL_ERROR "Your compiler does not support C++ coroutines;
            the earliest GCC version to do so is GCC 10.")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
else ()
    message(FATAL_ERROR "Your compiler has no known way of enabling C++ coroutines.")
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE STRING
        "Install path prefix, prepended onto install directories." FORCE)
endif()

include(GNUInstallDirs)

set(NONSENSE_CONFIG_PREFIX ${CMAKE_INSTALL_FULL_SYSCONFDIR} CACHE STRING
    "The prefix for the configuration directory.")
set(NONSENSE_DBUS_CONFIG_PREFIX "${CMAKE_INSTALL_FULL_DATADIR}/dbus-1" CACHE STRING
    "The prefix for the dbus configuration directory.")
set(NONSENSE_SYSTEMD_SYSTEM_PREFIX "/lib/systemd/system" CACHE STRING
    "The prefix for the systemd unit directory to use.")

find_package(PkgConfig REQUIRED)

pkg_check_modules(SYSTEMD libsystemd>=242)
if (NOT SYSTEMD_FOUND)
    message(FATAL_ERROR "libsystemd of version at least 242 is required, but has not been found.")
endif()

include_directories(
    SYSTEM
    vendor
    ${SYSTEMD_INCLUDE_DIRS}
)

link_directories(
    ${SYSTEMD_LIBRARY_DIRS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-private-field")
endif()

add_subdirectory(daemon)        # for nonsensed
add_subdirectory(entityd)       # for entityd
add_subdirectory(control)       # for nonsensectl
add_subdirectory(dbus)          # for install targets of dbus configuration files
add_subdirectory(systemd)       # for install targets of systemd units
add_subdirectory(etc)           # for install targets of nft configuration for the router unit
