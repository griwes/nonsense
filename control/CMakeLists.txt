file(
    GLOB sources
    CONFIGURE_DEPENDS
    *.cpp
)

add_executable(
    nonsensectl
    ${sources}
)

set_target_properties(
    nonsensectl
    PROPERTIES
        COMPILE_FLAGS "${SYSTEMD_CFLAGS}"
)

target_link_libraries(
    nonsensectl
    ${SYSTEMD_LDFLAGS}
)

install(
    TARGETS
        nonsensectl
    DESTINATION
        bin
)
