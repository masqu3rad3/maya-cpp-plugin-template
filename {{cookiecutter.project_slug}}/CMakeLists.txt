cmake_minimum_required(VERSION 3.15)

# Plugin name
set(PROJECT_NAME {{ cookiecutter.project_slug }} )
project(${PROJECT_NAME})

# Optional Maya version from CLI
if(NOT DEFINED MAYA_VERSION)
  message(FATAL_ERROR "MAYA_VERSION not set. Use -DMAYA_VERSION={{ cookiecutter.maya_version }}")
endif()

# Paths
if(NOT DEFINED MAYA_DEVKIT_ROOT)
  set(MAYA_DEVKIT_ROOT "${CMAKE_SOURCE_DIR}/{{ cookiecutter.devkit_directory }}/${MAYA_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 17)

# Include directories
include_directories("${MAYA_DEVKIT_ROOT}/include")

# Library directories
link_directories("${MAYA_DEVKIT_ROOT}/lib")

# Platform-specific settings
if(WIN32)
    add_definitions(-DNOMINMAX -DNT_PLUGIN -D_BOOL)
    set(EXTENSION "mll")
elseif(APPLE)
    add_definitions(-D_BOOL)
    set(EXTENSION "bundle")
else()
    set(EXTENSION "so")
endif()

# Sources
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

# Build plugin
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

# Link Maya libs
target_link_libraries(${PROJECT_NAME}
    OpenMaya
    OpenMayaRender      # needed for MRenderUtil::sampleShadingNetwork
    OpenMayaUI          # needed for M3dView::active3dView
    Foundation
	OpenMayaAnim
    OpenMayaFX
    Foundation
)

# Output settings
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".${EXTENSION}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
)
