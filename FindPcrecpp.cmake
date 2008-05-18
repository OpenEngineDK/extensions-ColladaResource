# - Try to find libpcrecpp
# Once done this will define
#
#  LIBPCRECPP_FOUND - system has libpcrecpp
#

FIND_LIBRARY(LIBPCRECPP
  NAMES 
  pcrecpp
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

IF(LIBPCRECPP)
   SET(LIBPCRECPP_FOUND TRUE)
   MARK_AS_ADVANCED(LIBPCRECPP)
ENDIF(LIBPCRECPP)
