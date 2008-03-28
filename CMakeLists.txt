
# Create the extension library
ADD_LIBRARY(Extensions_ColladaResource
  Resources/ColladaResource.cpp
#  Resources/intGeometry.cpp
)

TARGET_LINK_LIBRARIES(Extensions_ColladaResource
  OpenEngine_Resources
  ${COLLADA_DOM_LIBRARIES}
  # Extension dependencies
)
