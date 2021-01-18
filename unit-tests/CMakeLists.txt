file(
    GLOB test_files
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../test/suite/1.unit/*.test
)

if (test_files)
    file(
        REMOVE
        ${test_files}
    )
endif()

option(ENABLE_TESTS "Enable unit test generation. (Will generate files in test/suite/1.unit.)" OFF)
if (ENABLE_TESTS)
    file(
        GLOB test_sources
        CONFIGURE_DEPENDS
        *.cpp
    )

    foreach (test_source IN LISTS test_sources)
        string(REGEX
            REPLACE ".*/([^/]+).cpp" "\\1"
            test
            ${test_source}
        )

        add_executable(
            unit-test-${test}
            ${test_source}
            $<TARGET_OBJECTS:nonsensed-objects>
        )

        target_link_libraries(
            unit-test-${test}
            ${SYSTEMD_LDFLAGS}
            ${CMAKE_DL_LIBS}
        )

        configure_file(
            template.test
            ${CMAKE_CURRENT_SOURCE_DIR}/../test/suite/1.unit/${test}.test
            @ONLY
        )
    endforeach()
endif()
