macro(install_symlink filepath sympath)
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filepath} $ENV{DESTDIR}${sympath})")
    install(CODE "message(STATUS \"Created symlink: $ENV{DESTDIR}${sympath} -> ${filepath}\")")
endmacro(install_symlink)


file(
    GLOB templates
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    *.in
)

file(
    GLOB units
    CONFIGURE_DEPENDS
    *.slice
    *.service
    *.target
)

foreach (template IN LISTS templates)
    string(REGEX
        REPLACE ".in$" ""
        unit
        ${template}
    )

    configure_file(
        ${template}
        ${unit}
        @ONLY
    )

    list(APPEND units "${CMAKE_CURRENT_BINARY_DIR}/${unit}")
endforeach()

install(
    FILES
        ${units}
    DESTINATION
        ${NONSENSE_SYSTEMD_SYSTEM_PREFIX}
)

install(
    DIRECTORY
        nonsense-.slice.d
    DESTINATION
        ${NONSENSE_SYSTEMD_SYSTEM_PREFIX}
)

install(
    PROGRAMS
        nonsense-connection-setup
        nonsense-connection-teardown
    DESTINATION
        bin
)

install_symlink(
    ${NONSENSE_SYSTEMD_SYSTEM_PREFIX}/nonsensed.service
    ${NONSENSE_SYSTEMD_SYSTEM_PREFIX}/dbus-info.griwes.nonsense.service
)
