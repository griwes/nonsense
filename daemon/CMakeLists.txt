file(
    GLOB sources
    CONFIGURE_DEPENDS
    *.cpp
)

add_executable(
    nonsensed
    ${sources}
)

set_target_properties(
    nonsensed
    PROPERTIES
        COMPILE_FLAGS "${SYSTEMD_CFLAGS}"
)

target_link_libraries(
    nonsensed
    ${SYSTEMD_LDFLAGS}
    ${CMAKE_DL_LIBS}
)

install(
    TARGETS
        nonsensed
    DESTINATION
        bin
)
