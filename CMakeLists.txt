cmake_minimum_required(VERSION 3.15)

project(nonsense CXX)

set(NONSENSE_CONFIG_PREFIX "/etc" CACHE STRING "The prefix for the configuration directory.")

find_package(PkgConfig REQUIRED)

pkg_check_modules(SYSTEMD libsystemd>=243)
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
