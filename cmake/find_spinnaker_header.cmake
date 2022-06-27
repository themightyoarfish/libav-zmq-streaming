SET(Spinnaker_HEADER "spinnaker/System.h")

FIND_PATH(Spinnaker_INCLUDE_DIR ${Spinnaker_HEADER} PATHS
	"${CMAKE_INSTALL_PREFIX}/include"
	/usr/include
	/usr/local/include
	DOC "The path to Spinnaker headers"
)

FIND_LIBRARY(Spinnaker_LIBRARY NAMES Spinnaker HINTS
	/usr/lib
	/usr/local/lib
)
