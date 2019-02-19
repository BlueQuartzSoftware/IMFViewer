#!/bin/bash

#------------------------------------------------------------------------------
# Don't forget to run
# sudo httpd
# To start up the local webserver on macOS
# A lot of the code was inspired from the project https://github.com/andreyvit/create-dmg

DREAM3D_SDK=/opt/DREAM3D_SDK
DREAM3D_SOURCE_ROOT=/home/$USER/DREAM3D-Dev/DREAM3D
BUILD_DIR=/home/$USER/DREAM3D-Dev/DREAM3D-Build

vers='6.5.104'
BUILD_FOLDER=zRel_$vers
volname=DREAM3D-${vers}
package_root=${BUILD_DIR}/${BUILD_FOLDER}/_CPack_Packages/Linux/TGZ
mount_name=${BUILD_DIR}/${BUILD_FOLDER}/_CPack_Packages/Linux/TGZ/${volname}
final_output_dir=${BUILD_DIR}/${BUILD_FOLDER}

mkdir -p ${BUILD_DIR}/${BUILD_FOLDER}
cd ${BUILD_DIR}/${BUILD_FOLDER}
rm -rf *
#------------------------------------------------------------------------------
# Setup all the proper CMake variables for a Release
# No python wrapping
# No QtWebEngine
# Build Docs
# mkdocs
#
CMAKE_ARGS="-DDREAM3D_SDK=${DREAM3D_SDK}"
CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_BUILD_TYPE=Release"
CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${mount_name}"
CMAKE_ARGS="${CMAKE_ARGS} -DSIMPL_USE_QtWebEngine=OFF"
CMAKE_ARGS="${CMAKE_ARGS} -DSIMPL_ENABLE_PYTHON=OFF"
CMAKE_ARGS="${CMAKE_ARGS} -DSIMPL_RELATIVE_PATH_CHECK=OFF"
CMAKE_ARGS="${CMAKE_ARGS} -DSIMPL_USE_MKDOCS=ON"
CMAKE_ARGS="${CMAKE_ARGS} -DBrandedSIMPLView_DIR=${DREAM3D_SOURCE_ROOT}/ExternalProjects/BrandedDREAM3D"
CMAKE_ARGS="${CMAKE_ARGS} -DITKImageProcessing_LeanAndMean=OFF"
CMAKE_ARGS="${CMAKE_ARGS} -DSIMPLView_BUILD_DOCUMENTATION=ON"


cmake -G Ninja ${CMAKE_ARGS} ${DREAM3D_SOURCE_ROOT}

ninja 

rm -rf ${BUILD_DIR}/${BUILD_FOLDER}/_CPack_Packages

cpack
