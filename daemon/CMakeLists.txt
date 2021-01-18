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

list(REMOVE_ITEM sources ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_library(
    nonsensed-objects
    OBJECT
    ${sources}
)

set_target_properties(
    nonsensed-objects
    PROPERTIES
        COMPILE_FLAGS "${SYSTEMD_CFLAGS} -I${CMAKE_CURRENT_BINARY_DIR}/generated"
)

add_executable(
    nonsensed
    main.cpp
    $<TARGET_OBJECTS:nonsensed-objects>
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
