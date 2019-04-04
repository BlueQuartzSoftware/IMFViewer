# IMF Viewer #
![](Resources/OpenSourceEdition/splash/IMFViewer_splash.png)

This project is sponsored by the US Air Force and US Navy Office of Scientific Development (OSD) that aims to allow users to view and generate multi-scale data sets primarily from optical and electron microscopes in the form of mosaics.

Application icon [Designed by Freepik](https://www.freepik.com/free-vector/set-of-abstract-modern-logos_1051962.htm)

# IMFViewer Build Notes #

## SDK Creation ##

    git clone -b topic/IMFViewer ssh://git@github.com/bluequartzsoftware/DREAM3DSuperbuild

+ VTK: The SDK compile does not currently compile VTK which will need to be done by the user. Version 8.2 is the recommended version of VTK
+ When complete go to the IMFViewer SDK directory that you setup and rename the DREAM3D_SDK.cmake file to IMFViewer_SDK.cmake

## Clone Repositories ##

IMFViewer uses git submodules so we just need to clone the single IMFViewer repository

    git clone -b feature/OSD_GUI ssh://git@github.com/bluequartzsoftware/IMFViewer

    git submodule init
    git submodule update

## Build ##

+ Create your build directory

        cmake(.exe) -G {Generator} -DIMFViewer_SDK={PATH TO SDK} -DVTK_DIR={PATH TO VTK BUILD} -DCMAKE_BUILD_TYPE={BUILD TYPE}

Then build with your generator of choice

