@echo off

if not exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" goto missing
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
@set ARCH_TYPE=x64
@set INTEL_ARCH_TYPE=intel64
goto :env_setup

:missing
echo The specified configuration type is missing.  The tools for the
echo configuration might not be installed.
goto :eof


:env_setup
for /f "delims=" %%i in ('hostname') do set HOSTNAME=%%i

@echo "%HOSTNAME%"

@set DREAM3D_SDK=C:\DREAM3D_SDK
:: Set the GIT_BRANCH to the proper branch
@set GIT_BRANCH=v6_5_104

@set CMAKE_INSTALL=%DREAM3D_SDK%\cmake-3.13.0-win64-x64
@echo CMAKE_INSTALL=%CMAKE_INSTALL%

@set EIGEN_INSTALL=%DREAM3D_SDK%\Eigen-3.3.5
@echo EIGEN_INSTALL=%EIGEN_INSTALL%

@set HDF5_INSTALL=%DREAM3D_SDK%\hdf5-1.10.3
@set HDF5_DIR=%DREAM3D_SDK%\hdf5-1.10.3\cmake\hdf5
@echo HDF5_INSTALL=%HDF5_INSTALL%

@set QWT_INSTALL=%DREAM3D_SDK%\Qwt-6.1.3-5.10.1
@echo QWT_INSTALL=%QWT_INSTALL%

@set Qt5_DIR=%DREAM3D_SDK%\Qt5.10.1\5.10.1\msvc2017_64
@echo Qt5_DIR=%Qt5_DIR%

@set ITK_DIR=%DREAM3D_SDK%\ITK-4.13.1
@set NINJA_INSTALL=C:\Applications\ninja-win
@echo NINJA_INSTALL=%NINJA_INSTALL%

@set SEVENZIP_INSTALL=C:\Program Files\7-Zip

@set PYTHON_INSTALL=C:\Applications\Anaconda3
@echo PYTHON_INSTALL=%PYTHON_INSTALL%

@set PATH=%PATH%;%PYTHON_INSTALL%;%PYTHON_INSTALL%\Scripts
@set PATH=%CMAKE_INSTALL%\bin;%PATH%;%Qt5_DIR%\bin;%HDF5_INSTALL%\bin
@SET PATH=%PATH%;%SEVENZIP_INSTALL%;%NINJA_INSTALL%


@set DREAM3D_SOURCE_ROOT=C:\Users\%USERNAME%\DREAM3D-Dev\DREAM3D
@echo DREAM3D_SOURCE_ROOT=%DREAM3D_SOURCE_ROOT%
@set DREAM3D_SOURCE_ROOT_CM=C:/Users/%USERNAME%/DREAM3D-Dev/DREAM3D

@set BUILD_DIR=C:\Users\%USERNAME%\DREAM3D-Dev\DREAM3D-Builds
@echo BUILD_DIR=%BUILD_DIR%

@set vers=6.5.104
@set BUILD_FOLDER=zRel-%vers%
@echo BUILD_FOLDER=%BUILD_FOLDER%


IF NOT EXIST %DREAM3D_SDK%\DREAM3D_Data\Data\SmallIN100\ (
  cd %DREAM3D_SDK%\DREAM3DData\Data\
  7z x SmallIN100.tar.gz
  7z x SmallIN100.tar
  del SmallIN100.tar
)

IF NOT EXIST %DREAM3D_SDK%\DREAM3D_Data\Data\Image\ (
  cd %DREAM3D_SDK%\DREAM3DData\Data\
  7z x Image.tar.gz
  7z x Image.tar
  del Image.tar
)


@echo "Starting Build...."
cd %BUILD_DIR%
IF EXIST %BUILD_FOLDER%\ (
    rd /S /Q %BUILD_FOLDER%
    mkdir %BUILD_FOLDER%
)

IF NOT EXIST %BUILD_FOLDER%\ (
  mkdir %BUILD_FOLDER%
)
cd %BUILD_FOLDER%

@set CMAKE_ARGS=-DDREAM3D_SDK=%DREAM3D_SDK%
@set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Release
@set CMAKE_ARGS=%CMAKE_ARGS% -DSIMPL_USE_QtWebEngine=OFF
@set CMAKE_ARGS=%CMAKE_ARGS% -DSIMPL_ENABLE_PYTHON=OFF
@set CMAKE_ARGS=%CMAKE_ARGS% -DSIMPL_RELATIVE_PATH_CHECK=OFF
@set CMAKE_ARGS=%CMAKE_ARGS% -DSIMPL_USE_MKDOCS=ON
@set CMAKE_ARGS=%CMAKE_ARGS% -DBrandedSIMPLView_DIR=%DREAM3D_SOURCE_ROOT_CM%/ExternalProjects/BrandedDREAM3D
@set CMAKE_ARGS=%CMAKE_ARGS% -DITKImageProcessing_LeanAndMean=OFF
@set CMAKE_ARGS=%CMAKE_ARGS% -DSIMPLView_BUILD_DOCUMENTATION=ON

::Configure the build
%CMAKE_INSTALL%/bin/cmake.exe -G Ninja %CMAKE_ARGS% %DREAM3D_SOURCE_ROOT%

::Compile the sources
%CMAKE_INSTALL%/bin/cmake.exe --build . --target all

::Package the build
%CMAKE_INSTALL%/bin/cpack.exe --verbose
