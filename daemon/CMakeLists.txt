set(NONSENSE_GENERATED_BANNER "/* !!! THIS IS A GENERATED FILE, DO NOT MODIFY !!! */")

configure_file(
    paths.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/nonsense-paths.h"
)

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
        COMPILE_FLAGS "${SYSTEMD_CFLAGS} -I${CMAKE_CURRENT_BINARY_DIR}/generated"
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
