file(
    GLOB files
    LIST_DIRECTORIES TRUE
    *
)

foreach (file IN LISTS files)
    if (IS_DIRECTORY "${file}")
        install(
            DIRECTORY
                "${file}"
            DESTINATION
                ${NONSENSE_CONFIG_PREFIX}/nonsense
        )
    endif()
endforeach()

