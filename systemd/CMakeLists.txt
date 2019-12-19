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
