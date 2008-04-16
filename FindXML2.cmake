# - Try to find libxml2
# Once done this will define
#
#  LIBXML2_FOUND - system has libxml2
#

FIND_LIBRARY(LIBXML2
  NAMES 
  xml2 
  PATHS
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/libraries
  ENV LD_LIBRARY_PATH
  ENV LIBRARY_PATH
  /usr/lib
  /usr/local/lib
  /opt/local/lib
  NO_DEFAULT_PATH
)

IF(LIBXML2)
   SET(LIBXML2_FOUND TRUE)
   MARK_AS_ADVANCED(LIBXML2)
ENDIF(LIBXML2)
