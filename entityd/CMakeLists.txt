file(
    GLOB sources
    CONFIGURE_DEPENDS
    *.cpp
)

add_executable(
    nonsense-entityd
    ${sources}
)

set_target_properties(
    nonsense-entityd
    PROPERTIES
        COMPILE_FLAGS "${SYSTEMD_CFLAGS}"
)

target_link_libraries(
    nonsense-entityd
    ${SYSTEMD_LDFLAGS}
    ${CMAKE_DL_LIBS}
)

install(
    TARGETS
        nonsense-entityd
    DESTINATION
        bin
)
