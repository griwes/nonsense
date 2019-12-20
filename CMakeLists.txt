cmake_minimum_required(VERSION 3.13)

project(nonsense CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)

set(NONSENSE_CONFIG_PREFIX "/etc" CACHE STRING "The prefix for the configuration directory.")
set(NONSENSE_DBUS_CONFIG_PREFIX "/etc/dbus-1/system.d" CACHE STRING
    "The prefix for the dbus configuration directory.")
set(NONSENSE_SYSTEMD_SYSTEM_PREFIX "/lib/systemd/system" CACHE STRING
    "The prefix for the systemd unit directory to use.")

find_package(PkgConfig REQUIRED)

pkg_check_modules(SYSTEMD libsystemd>=241)
if (NOT SYSTEMD_FOUND)
    message(FATAL_ERROR "libsystemd of version at least 243 is required, but has not been found.")
endif()

include_directories(
    SYSTEM
    vendor
    ${SYSTEMD_INCLUDE_DIRS}
)

link_directories(
    ${SYSTEMD_LIBRARY_DIRS}
)

add_subdirectory(daemon)        # for nonsensed
add_subdirectory(control)       # for nonsensectl
add_subdirectory(dbus)          # for install targets of dbus configuration files
add_subdirectory(systemd)       # for install targets of systemd units
add_subdirectory(etc)           # for install targets of nft configuration for the router unit
