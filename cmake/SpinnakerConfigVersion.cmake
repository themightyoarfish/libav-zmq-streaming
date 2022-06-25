include(find_spinnaker_header)

# Now find the version string
EXECUTE_PROCESS(COMMAND bash -c "grep FLIR_SPINNAKER_VERSION_ ${Spinnaker_INCLUDE_DIR}/${Spinnaker_HEADER} | grep -o '[[:digit:]]\\+' | tr '\\n' '.' | sed 's/\.$//g'"
    OUTPUT_VARIABLE Spinnaker_VERSION
    COMMAND_ECHO STDOUT
    )

# the spinnaker version is something like 1.26.0.31, which stands for major,
# minor, type and build. no idea what that means.
message(STATUS "Spinnaker Version: ${Spinnaker_VERSION}")
string(REPLACE "." ";" Spinnaker_VERSION_LIST ${Spinnaker_VERSION})
list(GET Spinnaker_VERSION_LIST 0 Spinnaker_VERSION_MAJOR)
list(GET Spinnaker_VERSION_LIST 1 Spinnaker_VERSION_MINOR)
list(GET Spinnaker_VERSION_LIST 2 Spinnaker_VERSION_PATCH)
list(GET Spinnaker_VERSION_LIST 3 Spinnaker_VERSION_TWEAK)

set(Spinnaker_VERSION_COUNT 4)
set(PACKAGE_VERSION ${Spinnaker_VERSION})

if(${Spinnaker_VERSION} VERSION_GREATER_EQUAL ${PACKAGE_FIND_VERSION})
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif()


if(Spinnaker_VERSION_MAJOR EQUAL PACKAGE_FIND_VERSION_MAJOR AND
        Spinnaker_VERSION_MINOR EQUAL PACKAGE_FIND_VERSION_MINOR)
    if(Spinnaker_VERSION_PATCH EQUAL PACKAGE_FIND_VERSION_PATCH AND
            Spinnaker_VERSION_TWEAK EQUAL PACKAGE_FIND_VERSION_TWEAK)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
