file(
    GLOB files
    LIST_DIRECTORIES TRUE
    CONFIGURE_DEPENDS
    *
)

foreach (file IN LISTS files)
    if (IS_DIRECTORY "${file}")
        install(
            DIRECTORY
                "${file}"
            DESTINATION
                ${NONSENSE_CONFIG_PREFIX}
        )
    endif()
endforeach()

