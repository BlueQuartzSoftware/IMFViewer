
if(NOT "${IMFViewer_SDK}" STREQUAL "")
  include("${IMFViewer_SDK}/IMFViewer_SDK.cmake")
  message(STATUS "IMFViewer_SDK Location: ${IMFViewer_SDK}")
endif()

# Request C++11 standard, using new CMake variables.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

if(${CMAKE_MAJOR_VERSION} STREQUAL "3")
  # CMake 3.0 introduces VERSION variables for each Project.
  cmake_policy(SET CMP0048 NEW)
  # CMake 3.0 introduces some OS X RPath policies
  cmake_policy(SET CMP0042 NEW)
  # CMake 3.1 introduces if() policies on dereferencing variables in quotes
  cmake_policy(SET CMP0054 NEW)
endif()

cmake_minimum_required(VERSION 3.13.0)

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

set(IMFViewer_ExternalProject_DIR "${IMFViewerProj_SOURCE_DIR}/Source/External_Projects")
set(IMFViewer_Plugins_DIR "${IMFViewerProj_SOURCE_DIR}/Source/Plugins")

get_filename_component(IMFViewer_PARENT ${IMFViewerProj_SOURCE_DIR} DIRECTORY CACHE)
set(UnitTestAnalysisFile ${IMFViewerProj_BINARY_DIR}/UnitTestAnalysis.md)
file(WRITE ${UnitTestAnalysisFile} "## Unit Test Analysis ##\n")
set(TotalFilters 0)
set(TotalTestedFilters 0)

#------------------------------------------------------------------------------
# If the developer has set another CMP directory then use that, otherwise look
# for the CMP directory in the ExternalProjects directory
if("${CMP_SOURCE_DIR}" STREQUAL "")
  if(EXISTS "${IMFViewer_ExternalProject_DIR}/CMP")
    set(CMP_SOURCE_DIR "${IMFViewer_ExternalProject_DIR}/CMP")
  else()
    message(FATAL_ERROR "CMP_SOURCE_DIR was not set.  Please set the CMP_SOURCE_DIR variable to the CMP directory.")
  endif()
  message(STATUS "CMP_SOURCE_DIR: ${CMP_SOURCE_DIR}")
endif()

#------------------------------------------------------------------------------
# If the developer has set another SIMPL directory then use that, otherwise look
# for the SIMPL directory in the ExternalProjects directory
if("${SIMPLProj_SOURCE_DIR}" STREQUAL "")
  if(EXISTS "${IMFViewer_ExternalProject_DIR}/SIMPL")
    set(SIMPLProj_SOURCE_DIR "${IMFViewer_ExternalProject_DIR}/SIMPL")
  else()
    message(FATAL_ERROR "SIMPLProj_SOURCE_DIR was not set. Please set the SIMPLProj_SOURCE_DIR variable to the SIMPL directory.")
  endif()
  message(STATUS "SIMPLProj_SOURCE_DIR: ${SIMPLProj_SOURCE_DIR}")
endif()

#------------------------------------------------------------------------------
# If the developer has set another SIMPL directory then use that, otherwise look
# for the SIMPL directory in the ExternalProjects directory
if("${SIMPLVtkLibProj_SOURCE_DIR}" STREQUAL "")
  if(EXISTS "${IMFViewer_ExternalProject_DIR}/SIMPLVtkLib")
    set(SIMPLVtkLibProj_SOURCE_DIR "${IMFViewer_ExternalProject_DIR}/SIMPLVtkLib")
  else()
    message(FATAL_ERROR "SIMPLVtkLibProj_SOURCE_DIR was not set. Please set the SIMPLVtkLibProj_SOURCE_DIR variable to the SIMPLVtkLib directory.")
  endif()
  message(STATUS "SIMPLVtkLibProj_SOURCE_DIR: ${SIMPLVtkLibProj_SOURCE_DIR}")
endif()


# -----------------------------------------------------------------------
# Setup a Global property that is used to gather Documentation Information
# into a single known location
get_property(IMFViewerDocRoot GLOBAL PROPERTY IMFViewerDocRoot)
if("${IMFViewerDocRoot}" STREQUAL "")
  set_property(GLOBAL PROPERTY IMFViewerDocRoot "${IMFViewerProj_BINARY_DIR}/Documentation")
  get_property(IMFViewerDocRoot GLOBAL PROPERTY IMFViewerDocRoot)
endif()


get_property(SIMPLPluginList GLOBAL PROPERTY SIMPLPluginList)
if("${SIMPLPluginList}" STREQUAL "")
  set_property(GLOBAL PROPERTY SIMPLPluginList "${IMFViewerProj_BINARY_DIR}/SIMPLPluginList.txt")
  get_property(SIMPLPluginList GLOBAL PROPERTY SIMPLPluginList)
  file(WRITE "${SIMPLPluginList}" "")
endif()


get_property(SIMPLibSearchDirs GLOBAL PROPERTY SIMPLibSearchDirs)
if("${SIMPLibSearchDirs}" STREQUAL "")
  set_property(GLOBAL PROPERTY SIMPLibSearchDirs "${IMFViewerProj_BINARY_DIR}/LibSearchDirs.txt")
  get_property(SIMPLibSearchDirs GLOBAL PROPERTY SIMPLibSearchDirs)
  file(WRITE "${SIMPLibSearchDirs}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY};")
endif()


# -----------------------------------------------------------------------
# Set a global value to NOT install headers
# -----------------------------------------------------------------------
set_property(GLOBAL PROPERTY CMP_PROJECT_INSTALL_HEADERS "0")

# -----------------------------------------------------------------------
# Set a Global Prefix where everthing is going to get intalled. For OS X we are
# going to try to install everything into the .app package
# -----------------------------------------------------------------------
set_property(GLOBAL PROPERTY DREAM3D_PACKAGE_DEST_PREFIX ".")
if(APPLE)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_property(GLOBAL PROPERTY DREAM3D_PACKAGE_DEST_PREFIX "${SIMPLView_APPLICATION_NAME}${EXE_DEBUG_EXTENSION}.app/Contents/")
  else()
    set_property(GLOBAL PROPERTY DREAM3D_PACKAGE_DEST_PREFIX "${SIMPLView_APPLICATION_NAME}.app/Contents/")
  endif()
endif()
get_property(DREAM3D_PACKAGE_DEST_PREFIX GLOBAL PROPERTY DREAM3D_PACKAGE_DEST_PREFIX)


# ------- Enable the CTest testing. ---------------
OPTION(SIMPL_BUILD_TESTING "Compile the test programs" ON)
# -------------------------------------------------------------------- 
# If Testing is enabled, turn on the Unit Tests 
if(SIMPL_BUILD_TESTING)
    #ENABLE_TESTING()
    include( CTest )
endif()



# --------------------------------------------------------------------
# This is set to "1" so that our test files are deleted at the end of the tests
set(DREAM3D_REMOVE_TEST_FILES "1")




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

set(CMP_CONFIGURED_FILES_SOURCE_DIR ${CMP_SOURCE_DIR}/ConfiguredFiles)

INCLUDE (${CMP_SOURCE_DIR}/cmpCMakeMacros.cmake )

set(CMP_ENABLE_PLUGINS "1")
set(CMP_PLUGIN_LIST_FILE "${SIMPLPluginList}")
set(CMP_PLUGIN_SEARCHDIR_FILE "${SIMPLibSearchDirs}")
file(APPEND ${CMP_PLUGIN_SEARCHDIR_FILE} "${PROJECT_BINARY_DIR}/Bin/plugins;")
file(APPEND ${CMP_PLUGIN_SEARCHDIR_FILE} "${PROJECT_BINARY_DIR}/Bin;")
set(CMP_LIB_SEARCH_DIRS "")
set(CMP_MODULES_SOURCE_DIR ${CMP_SOURCE_DIR}/Modules CACHE INTERNAL "")

if(UNIX AND NOT APPLE)
    set(IMFViewerProj_LINUX_SYSTEM ${CMAKE_SYSTEM_NAME} CACHE STRING "The Name of the linux distrubution for packaging")
endif()

#------------------------------------------------------------------------------
# If we have not defined a "BrandedIMFViewer_DIR" then define it to the default
# generic set of ICONs
if(NOT DEFINED BrandedIMFViewer_DIR)
  set(BrandedIMFViewer_DIR ${IMFViewerProj_SOURCE_DIR}/Resources/OpenSourceEdition)
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
  message(FATAL_ERROR "BUILD_SHARED_LIBS Must be set to OFF in order to build properly.")
endif(BUILD_SHARED_LIBS)




# Find and Use the Qt5 Libraries
include(${CMP_SOURCE_DIR}/ExtLib/Qt5Support.cmake)
set(IMFViewer_Qt5_Components Core Widgets Network Gui Concurrent Svg Xml OpenGL PrintSupport )
# option(SIMPLView_USE_QtWebEngine "Use QWebEngine to display Help. OFF=Use default web browser" ON)
CMP_AddQt5Support( "${IMFViewer_Qt5_Components}"
                    "OFF"
                    "${IMFViewerProj_BINARY_DIR}"
                    "IMFViewer")


# ---------- Find HDF5 Headers/Libraries -----------------------
# HDF5 now comes with everything that is needed for CMake to load
# up the targets (Exported) that it needs. We just need to find where
# HDF5 is installed.
include(${CMP_SOURCE_DIR}/ExtLib/HDF5Support.cmake)

# --------------------------------------------------------------------
# Look for Vtk 8.0 as we need it for the plugin GUI to be generated
# These are the required component libraries
set(VtkComponents
    vtkFiltersTexture
    vtkGUISupportQt
    vtkRenderingCore
    vtkRenderingFreeType
    vtkRenderingOpenGL2
    vtkRenderingVolumeOpenGL2
    vtkRenderingContextOpenGL2
    vtkRenderingAnnotation
    vtkInteractionWidgets
    vtkInteractionStyle
    vtkIOLegacy
    vtkIOImage
    vtkIOGeometry
    vtkChartsCore
    vtkViewsContext2D
  )

include("${CMP_SOURCE_DIR}/ExtLib/VtkSupport.cmake")
CMP_FindVtkComponents(QT5_REQUIRED TRUE
                      COMPONENTS ${VtkComponents}
                      )

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
# Add in the SIMPL Sources
# -----------------------------------------------------------------------
add_subdirectory( ${SIMPLProj_SOURCE_DIR} ${PROJECT_BINARY_DIR}/SIMPL)

# -----------------------------------------------------------------------
# Add in the SIMPLVtkLib Sources
# -----------------------------------------------------------------------
add_subdirectory( ${SIMPLVtkLibProj_SOURCE_DIR} ${PROJECT_BINARY_DIR}/SIMPLVtkLib)

# -----------------------------------------------------------------------
# Add in the Necessary IMFViewer Sources
# -----------------------------------------------------------------------
add_subdirectory( ${IMFViewerProj_SOURCE_DIR}/Source/Applications/IMFViewer ${PROJECT_BINARY_DIR}/Applications/IMFViewer)

#-------------------------------------------------------------------------------
# Compile the Core Plugins that come with IMFViewer and any other Plugins that the
# developer has added.
include(${PROJECT_SOURCE_DIR}/Source/Plugins/Plugins.cmake)

# This should be the last line in this file:
include(${PROJECT_RESOURCES_DIR}/CPack/PackageProject.cmake)

