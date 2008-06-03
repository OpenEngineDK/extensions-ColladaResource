SET(PCRELIB "pcre")
SET(PCRECPPLIB "pcrecpp")
SET(LIBXML2LIB "libxml2")

IF(CMAKE_BUILD_TOOL MATCHES "(msdev|devenv|nmake)")
  SET(LIBXML2LIB "libxml2_a")
  STRING(COMPARE EQUAL ${CMAKE_BUILD_TYPE} "debug" ISDEBUGENABLED)
  IF(ISDEBUGENABLED)
    SET(PCRELIB "pcre-d")
    SET(PCRECPPLIB "pcrecpp-d")
  ELSE(ISDEBUGENABLED)
    SET(PCRELIB "pcre")
    SET(PCRECPPLIB "pcrecpp")
  ENDIF(ISDEBUGENABLED)
ENDIF(CMAKE_BUILD_TOOL MATCHES "(msdev|devenv|nmake)")

FIND_LIBRARY(LIBPCRE
  NAMES 
  ${PCRELIB}
  PATHS
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/libraries
  ${PROJECT_SOURCE_DIR}/libraries/colladadom2.1/lib
  ENV LD_LIBRARY_PATH
  ENV LIBRARY_PATH
  /usr/lib
  /usr/local/lib
  /opt/local/lib
  NO_DEFAULT_PATH
)

IF(NOT LIBPCRE)
  MESSAGE ("WARNING: Could not find pcre - depending targets will be disabled.")
ENDIF(NOT LIBPCRE)

FIND_LIBRARY(LIBPCRECPP
  NAMES 
  ${PCRECPPLIB}
  PATHS
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/libraries
  ${PROJECT_SOURCE_DIR}/libraries/colladadom2.1/lib
  ENV LD_LIBRARY_PATH
  ENV LIBRARY_PATH
  /usr/lib
  /usr/local/lib
  /opt/local/lib
  NO_DEFAULT_PATH
)

IF(NOT LIBPCRECPP)
  MESSAGE ("WARNING: Could not find pcrecpp - depending targets will be disabled.")
ENDIF(NOT LIBPCRECPP)

FIND_LIBRARY(LIBXML2
  NAMES 
  ${LIBXML2LIB}
  PATHS
  ${PROJECT_SOURCE_DIR}/libraries/colladadom2.1/lib
  NO_DEFAULT_PATH
)

IF(NOT LIBXML2)
  MESSAGE ("WARNING: Could not find libxml2 - depending targets will be disabled.")
ENDIF(NOT LIBXML2)

FIND_LIBRARY(LIBZLIB
  NAMES 
  zlib
  PATHS
  ${PROJECT_SOURCE_DIR}/libraries/colladadom2.1/lib
  NO_DEFAULT_PATH
)

IF(NOT LIBZLIB)
  MESSAGE ("WARNING: Could not find zlib - depending targets will be disabled.")
ENDIF(NOT LIBZLIB)

# Create the extension library
ADD_LIBRARY(Extensions_ColladaResource
  Resources/ColladaResource.cpp
#  Resources/intGeometry.cpp
)

TARGET_LINK_LIBRARIES(Extensions_ColladaResource
  OpenEngine_Resources
  # Extension dependencies
  ${COLLADA_DOM_LIBRARIES}
  ${LIBZLIB}
  ${LIBXML2}
  ${LIBPCRECPP}
  ${LIBPCRE}
)
