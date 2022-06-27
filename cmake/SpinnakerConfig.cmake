include(find_spinnaker_header)

IF(OSX)
    FIND_LIBRARY(OMP_FOUND omp)
    if(OMP_FOUND STREQUAL OMP_FOUND-NOTFOUND)
        message(FATAL_ERROR "OpenMP not found. Maybe run `brew install libomp`?")
    ENDIF()
ENDIF()

SET(Spinnaker_FOUND "NO")

IF(Spinnaker_INCLUDE_DIR)
	IF(Spinnaker_LIBRARY)
		SET(Spinnaker_INCLUDE_DIR "${Spinnaker_INCLUDE_DIR}/spinnaker")
        set(Spinnaker_LIBRARIES ${Spinnaker_LIBRARY})
		SET(Spinnaker_FOUND "YES")
	ELSE()
		MESSAGE(FATAL_ERROR
            "Includes dir is missing:
            ${Spinnaker_INCLUDE_DIR}/${Spinnaker_HEADER}
            Sometimes is because some paths changed (from /usr to /usr/local).
            Make sure that you rm -rf CMakeCache.txt; make rebuild_cache;")
    ENDIF()
ENDIF()
