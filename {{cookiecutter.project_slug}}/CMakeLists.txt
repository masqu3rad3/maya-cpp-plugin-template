cmake_minimum_required(VERSION 3.15)

project({{ cookiecutter.project_slug }} LANGUAGES CXX)

# # Paths
if(NOT DEFINED MAYA_DEVKIT_ROOT)
  set(MAYA_DEVKIT_ROOT "${CMAKE_SOURCE_DIR}/{{ cookiecutter.devkit_directory }}/${MAYA_VERSION}/devkitBase")
endif()

# Plugin Subdirectories
