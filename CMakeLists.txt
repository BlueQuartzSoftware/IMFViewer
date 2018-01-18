#--////////////////////////////////////////////////////////////////////////////
#--
#--  Copyright (c) 2009, Michael A. Jackson. BlueQuartz Software
#--  Copyright (c) 2009, Michael Groeber, US Air Force Research Laboratory
#--  All rights reserved.
#--  BSD License: http://www.opensource.org/licenses/bsd-license.html
#--
#-- This code was partly written under US Air Force Contract FA8650-07-D-5800
#--
#--////////////////////////////////////////////////////////////////////////////


if(NOT "${DREAM3D_SDK}" STREQUAL "")
  include("${DREAM3D_SDK}/DREAM3D_SDK.cmake")
  message(STATUS "DREAM3D_SDK Location: ${DREAM3D_SDK}")
endif()

if(${CMAKE_MAJOR_VERSION} STREQUAL "3")
  # CMake 3.0 introduces VERSION variables for each Project.
  cmake_policy(SET CMP0048 NEW)
  # CMake 3.0 introduces some OS X RPath policies
  cmake_policy(SET CMP0042 OLD)
  # CMake 3.1 introduces if() policies on dereferencing variables in quotes
  cmake_policy(SET CMP0054 NEW)
endif()

cmake_minimum_required(VERSION 3.8.0)

#------------------------------------------------------------------------------
# Create the Top level Project
project(IMFViewerProj VERSION 1.0.0.0)

#------------------------------------------------------------------------------
# There are several different Release Types that we are going to use.
# Official: This is an official release from BlueQuartz Software
# Beta: This is a Beta release that has is getting ready to be officially released.
# Development: This is currently in development and probably has bugs.
set(IMFViewerProj_RELEASE_TYPE "Development")

include(CMakeParseArguments)

# ---------- Setup output Directories -------------------------
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
  ${PROJECT_BINARY_DIR}/Bin
  CACHE PATH
  "Single Directory for all Libraries"
  )

# --------- Setup the Executable output Directory -------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
  ${PROJECT_BINARY_DIR}/Bin
  CACHE PATH
  "Single Directory for all Executables."
  )

# --------- Setup the Executable output Directory -------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
  ${PROJECT_BINARY_DIR}/Bin
  CACHE PATH
  "Single Directory for all static libraries."
  )

if(MSVC_IDE OR XCODE)
  file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)
  file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release)
endif()

#Enable use of Solution Folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#set_property(GLOBAL PROPERTY USE_FOLDERS OFF)
set(PROJECT_CODE_DIR ${PROJECT_SOURCE_DIR}/Source)
set(PROJECT_RESOURCES_DIR ${IMFViewerProj_SOURCE_DIR}/Resources)
set(EXE_DEBUG_EXTENSION "_debug")
set(PROJECT_INSTALL_HEADERS "0")
set(PROJECT_INSTALL_EXECUTABLES "1")
set(IMFViewerProj_DOCUMENTATION_DIR ${PROJECT_SOURCE_DIR}/Documentation)
set(IMFViewerProj_SUPPORT_DIR ${PROJECT_SOURCE_DIR}/Support)

set(CMP_SOURCE_DIR ${IMFViewerProj_SOURCE_DIR}/Support/cmp)
set(CMP_CONFIGURED_FILES_SOURCE_DIR ${CMP_SOURCE_DIR}/ConfiguredFiles)
INCLUDE (${CMP_SOURCE_DIR}/cmpCMakeMacros.cmake )
set(CMP_ENABLE_PLUGINS "1")
set(CMP_PLUGIN_LIST_FILE ${PROJECT_BINARY_DIR}/IMFViewerProj_PluginList.txt)
file(WRITE ${CMP_PLUGIN_LIST_FILE} "")
set(CMP_PLUGIN_SEARCHDIR_FILE ${PROJECT_BINARY_DIR}/libsearchdirs.txt)
file(WRITE ${CMP_PLUGIN_SEARCHDIR_FILE} "${PROJECT_BINARY_DIR}/Bin/plugins;")
file(APPEND ${CMP_PLUGIN_SEARCHDIR_FILE} "${PROJECT_BINARY_DIR}/Bin;")
set(CMP_LIB_SEARCH_DIRS "")
set(CMP_MODULES_SOURCE_DIR ${CMP_SOURCE_DIR}/Modules CACHE INTERNAL "")

if(UNIX AND NOT APPLE)
    set(IMFViewerProj_LINUX_SYSTEM ${CMAKE_SYSTEM_NAME} CACHE STRING "The Name of the linux distrubution for packaging")
endif()

#------------------------------------------------------------------------------
# Find the Git Package for Versioning. It should be ok if Git is NOT found
Find_package(Git)

cmpRevisionString( GENERATED_HEADER_FILE_PATH "FileNotUsed.h"
                        GENERATED_SOURCE_FILE_PATH "FileNotUsed.cpp"
                        NAMESPACE "IMFViewerProj"
                        PROJECT_NAME "${PROJECT_NAME}"
                        EXPORT_MACRO "IMFViewerProj_EXPORT")

# --------------------------------------------------------------------
# Over ride CMake's built in module directory by prepending cmp's module
# directory first
set(CMAKE_MODULE_PATH ${CMP_MODULES_SOURCE_DIR} ${CMAKE_MODULE_PATH} ${Qt5_DIR})

# Build shared libraries
OPTION (BUILD_SHARED_LIBS "Build Shared Libraries" ON)
set(LIB_TYPE STATIC)
if(BUILD_SHARED_LIBS)
  #message(FATAL_ERROR "Due to issues exporting symbols with MSVC IMFViewerProj does NOT currently support intermediate shared libraries. Please turn OFF BUILD_SHARED_LIBS")
    set(LIB_TYPE SHARED)
    list(APPEND CMP_LIB_SEARCH_DIRS  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
    if(MSVC)
        list(APPEND CMP_LIB_SEARCH_DIRS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug
                                        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release )
    endif()

else()
  message(FATAL_ERROR "BUILD_SHARED_LIBS Must be set to ON in order to build properly.")
endif(BUILD_SHARED_LIBS)


# ---------- Find and Use the Qt5 Libraries -----------------------
include(${CMP_SOURCE_DIR}/ExtLib/Qt5Support.cmake)


# ---------- Find HDF5 Headers/Libraries -----------------------
# HDF5 now comes with everything that is needed for CMake to load
# up the targets (Exported) that it needs. We just need to find where
# HDF5 is installed.
include(${CMP_SOURCE_DIR}/ExtLib/HDF5Support.cmake)

# --------------------------------------------------------------------
# Now that we have found all of our required libraries and packages we can start the all
# the tests

# We do NOT want to install any of the headers
set(IMFViewerProj_INSTALL_FILES "${PROJECT_INSTALL_HEADERS}")

# --------------------------------------------------------------------
# Generate our ReadMe and License Files
configure_file(${PROJECT_SOURCE_DIR}/ReadMe.md
                ${PROJECT_BINARY_DIR}/ReadMe.md )

# --------------------------------------------------------------------
# Generate install rules for the text files
install(FILES ${PROJECT_BINARY_DIR}/ReadMe.md
        DESTINATION .
        COMPONENT Applications)

# Include some directories for the compiler
include_directories(${PROJECT_BINARY_DIR})

# --------------------------------------------------------------------
# Add in some compiler definitions
# --------------------------------------------------------------------
if( CMAKE_BUILD_TYPE MATCHES Debug )
  ADD_DEFINITIONS(-DDEBUG)
ENDif( CMAKE_BUILD_TYPE MATCHES Debug )

if(NOT MSVC)
 # ADD_DEFINITIONS(-Wall)
endif()

# On linux we need to set this because some of the libraries are Static
# and some are shared.
if( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" AND NOT MSVC )
  ADD_DEFINITIONS(-fPIC)
ENDif()

# --------------------------------------------------------------------
# If was are using GCC, make the compiler messages on a single line
if(CMAKE_COMPILER_IS_GNUCC)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fmessage-length=0")
ENDif(CMAKE_COMPILER_IS_GNUCC)
if(CMAKE_COMPILER_IS_GNUCXX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmessage-length=0")
ENDif(CMAKE_COMPILER_IS_GNUCXX)

if(MSVC)
 add_definitions(-D_CRT_SECURE_NO_WARNINGS)
 add_definitions(-D_SCL_SECURE_NO_WARNINGS)
 option(IMFViewerProj_DISABLE_MSVC_WARNINGS "Disable some MSVC Compiler warnings" OFF)
endif()



include_directories(${IMFViewerProj_SOURCE_DIR}/Source)
include_directories(${PROJECT_BINARY_DIR})

# -----------------------------------------------------------------------
# Add in the Necessary SIMPL Sources
# -----------------------------------------------------------------------
add_subdirectory( ${IMFViewerProj_SOURCE_DIR}/Source/SIMPL ${PROJECT_BINARY_DIR}/SIMPL)

#-------------------------------------------------------------------------------
add_subdirectory( ${IMFViewerProj_SOURCE_DIR}/Source/Applications/IMFViewer ${PROJECT_BINARY_DIR}/Applications/IMFViewer)


# This should be the last line in this file:
include(${PROJECT_RESOURCES_DIR}/CPack/PackageProject.cmake)

