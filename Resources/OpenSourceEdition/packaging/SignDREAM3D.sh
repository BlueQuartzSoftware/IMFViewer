#!/bin/bash

#------------------------------------------------------------------------------
# Don't forget to run
# sudo httpd
# To start up the local webserver on macOS
# A lot of the code was inspired from the project https://github.com/andreyvit/create-dmg

IMFViewer_SDK=/Users/Shared/IMFViewer_SDK
IMFViewer_SOURCE_ROOT=/Users/$USER/IMFViewer-Dev/IMFViewer
BUILD_DIR=/Users/$USER/IMFViewer-Dev/IMFViewer-Build
vers='1.0.0'
BUILD_FOLDER=zRel_$vers
volname=IMFViewer-${vers}-OSX
package_root=${BUILD_DIR}/${BUILD_FOLDER}/_CPack_Packages/Darwin/Install
dmgImage=${package_root}/${volname}.dmg
mount_name=${BUILD_DIR}/${BUILD_FOLDER}/_CPack_Packages/Darwin/Install/${volname}
final_output_dir=${BUILD_DIR}/${BUILD_FOLDER}

# This bit of code is here to make iterating on the Applescript portion quicker.
# It should ALWAYS be set to FALSE if you are just running this script to generate
# the DMG file
SKIP=FALSE
if [ "x$SKIP" = "xFALSE" ]; then

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
  CMAKE_ARGS="-DDREAM3D_SDK=${IMFViewer_SDK}"
  CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_BUILD_TYPE=Release"
  CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${mount_name}"
  CMAKE_ARGS="${CMAKE_ARGS} -DSIMPL_USE_QtWebEngine=OFF"
  CMAKE_ARGS="${CMAKE_ARGS} -DSIMPL_ENABLE_PYTHON=OFF"
  CMAKE_ARGS="${CMAKE_ARGS} -DSIMPL_RELATIVE_PATH_CHECK=ON"
  CMAKE_ARGS="${CMAKE_ARGS} -DSIMPL_USE_MKDOCS=ON"
  CMAKE_ARGS="${CMAKE_ARGS} -DBrandedSIMPLView_DIR=${IMFViewer_SOURCE_ROOT}/Resources/OpenSourceEdition"
  CMAKE_ARGS="${CMAKE_ARGS} -DITKImageProcessing_LeanAndMean=OFF"
  CMAKE_ARGS="${CMAKE_ARGS} -DSIMPLView_BUILD_DOCUMENTATION=ON"


  cmake -G Ninja ${CMAKE_ARGS} ${IMFViewer_SOURCE_ROOT}

  rm -rf ${BUILD_DIR}/${BUILD_FOLDER}/_CPack_Packages
  rm  ${BUILD_DIR}/${BUILD_FOLDER}/*.dmg

  ninja install
  #cpack

  cd ${mount_name}

  cd ${mount_name}/IMFViewer.app/Contents/Frameworks/QtCore.framework
  rm Headers QtCore_debug QtCore_debug.prl QtCore.prl
  cd ${mount_name}/IMFViewer.app/Contents/Frameworks/QtNetwork.framework
  rm Headers QtNetwork_debug QtNetwork_debug.prl QtNetwork.prl

  echo "Moving the Data folder...."
  cd ${mount_name}
  mkdir IMFViewerData
  mv Data IMFViewerData/.

  echo "Creating Applications symlink..."
  ln -s /Applications Applications

  cd ${BUILD_DIR}/${BUILD_FOLDER}

  #------------------------------------------------------------------------------
  # Do the actual Signing of the application
  echo "##########################################################################"
  echo "#-- Signing the IMFViewer Application Package....."
  cert_name='Developer ID Application: XXXXXXXXXXX (YYYYYYYYYY)'
  codesign --signature-size=99000 --verbose --deep -s "$cert_name" "${mount_name}/IMFViewer.app"

  echo "##########################################################################"
  echo "#-- Creating Disk Image"
  hdiutil create -fs HFS+ ${dmgImage} -format UDZO -imagekey zlib-level=9 -ov -volname "${volname}" -srcfolder ${mount_name}

fi

VOLUME_NAME="IMFViewer-${vers}-OSX"
MOUNT_DIR="/Volumes/${VOLUME_NAME}"
# try unmount dmg if it was mounted previously (e.g. developer mounted dmg, installed app and forgot to unmount it)
echo "Unmounting disk image..."
DEV_NAME=$(hdiutil info | egrep '^/dev/' | sed 1q | awk '{print $1}')
test -d "${MOUNT_DIR}" && hdiutil detach "${DEV_NAME}"


echo "##########################################################################"
echo "#-- Converting Disk image to RW...."
DMG_TEMP_NAME=${package_root}/tmp-udrw.dmg
echo "#-- Removing previous RW disk image..."
if [[ -e ${DMG_TEMP_NAME} ]]; then
  rm ${DMG_TEMP_NAME}
fi

# Convert from read-only original image to read-write.
cd ${package_root}

echo "##########################################################################"
echo "#-- Creating RW Disk Image...."
hdiutil convert "${dmgImage}" -format UDRW -o ${DMG_TEMP_NAME}
#rm "${dmgImage}"

# mount it
echo "##########################################################################"
echo "#-- Mounting disk image..."
echo "#-- Mount directory: $MOUNT_DIR"
DEV_NAME=$(hdiutil attach -readwrite -noverify -noautoopen "${DMG_TEMP_NAME}" | egrep '^/dev/' | sed 1q | awk '{print $1}')
echo "#-- Device name:     $DEV_NAME"
secs=$((4 * 1))
while [ $secs -gt 0 ]; do
  echo -ne "Waiting to Attach disk image: $secs\033[0K\r"
  sleep 1
  : $((secs--))
done

BACKGROUND_FILE="${IMFViewer_SOURCE_ROOT}/Resources/OpenSourceEdition/packaging/IMFViewer_DMGBackground.png"
BACKGROUND_FILE_NAME="$(basename $BACKGROUND_FILE)"
if ! test -z "$BACKGROUND_FILE"; then
  echo "Copying background file..."
  test -d "$MOUNT_DIR/.background" || mkdir "$MOUNT_DIR/.background"
  cp "$BACKGROUND_FILE" "$MOUNT_DIR/.background/$BACKGROUND_FILE_NAME"
fi

echo "##########################################################################"
echo "#-- Running Apple Script"
# The Apple Script will modify the disk image
osascript ${IMFViewer_SOURCE_ROOT}/Resources/CPack/IMFViewer_DMGSetup.scpt ${VOLUME_NAME}
secs=$((5 * 1))
while [ $secs -gt 0 ]; do
  echo -ne "Waiting to Detach disk image: $secs\033[0K\r"
  sleep 1
  : $((secs--))
done

# make sure it's not world writeable
echo "Fixing permissions on ${MOUNT_DIR}"
chmod -Rf go-w "${MOUNT_DIR}" &> /dev/null || true
echo "Done fixing permissions."

# make the top window open itself on mount:
echo "Blessing started of ${MOUNT_DIR}"
bless --folder "${MOUNT_DIR}" --openfolder "${MOUNT_DIR}"
echo "Blessing finished"

# Unmount the disk image
echo "##########################################################################"
echo "#-- Umounting disk image /Volumes/${VOLUME_NAME}"
hdiutil unmount /Volumes/${VOLUME_NAME} -force
hdiutil detach "$DEV_NAME" 

# Convert back to read-only, compressed image.
echo "##########################################################################"
echo "#-- Converting Disk Image to read-only format... "
hdiutil flatten ${DMG_TEMP_NAME} 
hdiutil convert ${DMG_TEMP_NAME} -format UDZO -imagekey zlib-level=9 -ov -o ${final_output_dir}/${volname}.dmg 

echo "#-- Removing Temp Disk Image"
rm ${DMG_TEMP_NAME}

#------------------------------------------------------------------------------
# With macOS 10.12 we need to sign the disk image as well.
echo "#-- Siging the entire DMG...."
codesign --force --signature-size=99000 --verbose --deep -s "$cert_name" ${final_output_dir}/${volname}.dmg

sudo spctl --master-enable
