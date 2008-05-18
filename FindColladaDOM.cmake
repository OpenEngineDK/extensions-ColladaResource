# - Try to find Collada DOM
# Once done this will define
#
#  COLLADA_DOM_FOUND - system has Collada dom
#  COLLADA_DOM_INCLUDE_DIR - the Collada DOM include directory
#  COLLADA_DOM_LIBRARIES - Link these to use Collada DOM
#

FIND_PATH(COLLADA_DOM_INCLUDE_DIR NAMES dae.h dom.h
  PATHS
  ${PROJECT_BINARY_DIR}/include
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/libraries/colladadom2.1/include
  ${PROJECT_SOURCE_DIR}/libraries/Collada14Dom.framework/Headers
  ENV CPATH
  /usr/include
  /usr/local/include
  /opt/local/include
  NO_DEFAULT_PATH
)

IF(${COLLADA_DOM_INCLUDE_DIR} MATCHES ".framework")
   # stolen from FindSDL.cmake
   STRING(REGEX REPLACE "(.*)/.*\\.framework/.*" "\\1" COLLADA_DOM_FRAMEWORK_PATH_TEMP ${COLLADA_DOM_INCLUDE_DIR})
   SET(COLLADA_DOM_LIBRARIES "-F${COLLADA_DOM_FRAMEWORK_PATH_TEMP} -framework Collada14dom")
ELSE(${COLLADA_DOM_INCLUDE_DIR} MATCHES ".framework")
 
FIND_LIBRARY(COLLADA_DOM_LIB
  NAMES 
  collada14dom-d  
  PATHS
  ${PROJECT_SOURCE_DIR}/libraries/colladadom2.1/lib/linux-1.4-d
  ENV LD_LIBRARY_PATH
  ENV LIBRARY_PATH
  /usr/lib
  /usr/local/lib
  /opt/local/lib
  NO_DEFAULT_PATH
)

SET (COLLADA_DOM_LIBRARIES
  ${COLLADA_DOM_LIB}
)

ENDIF(${COLLADA_DOM_INCLUDE_DIR} MATCHES ".framework")

IF(COLLADA_DOM_INCLUDE_DIR AND COLLADA_DOM_LIBRARIES)
   SET(COLLADA_DOM_INCLUDE_DIR
       ${COLLADA_DOM_INCLUDE_DIR}
       ${COLLADA_DOM_INCLUDE_DIR}/1.4
   )
   SET(COLLADA_DOM_FOUND TRUE)
ENDIF(COLLADA_DOM_INCLUDE_DIR AND COLLADA_DOM_LIBRARIES)

# show the COLLADA_DOM_INCLUDE_DIR and COLLADA_DOM_LIBRARIES variables only in the advanced view
IF(COLLADA_DOM_FOUND)
  MARK_AS_ADVANCED(COLLADA_DOM_INCLUDE_DIR COLLADA_DOM_LIBRARIES )
ENDIF(COLLADA_DOM_FOUND)


