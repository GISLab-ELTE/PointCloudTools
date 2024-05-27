# Quick Start for Window OS

This guide describes how to install the dependencies and compile *PointCloudTools* on Windows OS
in a quick and easy way. It is assumed that you already have Visual Studio 2019/2022 with the
*Desktop development with C++* workload installed.


## Install dependencies

### Install Boost

Download the official prebuilt Boost binaries from the following repository:  
https://sourceforge.net/projects/boost/files/boost-binaries/

Select a recent version (minimum requirement is 1.58), compatible with your MSVC version (Visual C++ 2022 is MSVC 14.3).  
64 bit version is preferred to be installed, but the 32 bit version can also be installed if required.

### Install GDAL

Download the unofficial prebuilt GDAL binaries for Windows from GISInternals:  
https://www.gisinternals.com/release.php

Select a recent version matching your Visual Studio / MSVC version.  
64 bit version is preferred to be installed, but the 32 bit version can also be installed if required.

*Note:* There is two separate download links. You have to download both *Compiled binaries* AND *Compiled libraries and headers*, then extract them to the same folder.

### Install OpenCV

Download the official prebuilt OpenCV binaries for Windows from their website:  
https://opencv.org/releases/

Select a recent version (minimum requirement is 4.2).  
The downloadable executable is a self-extracting archive, which you can extract at any preferred location.

*Note:* for more recent versions, only the 64 bit version of OpenCV is shipped in this self-extracting archive, but that should be preferred anyway.

### Install MS-MPI *(optional)*

Download the official prebuilt binaries for Microsoft's implementation of the MPI library  from the follwoing website:  
https://learn.microsoft.com/en-us/message-passing-interface/microsoft-mpi

For development, you especially need to install the SDK, execute the `msmpisdk.msi` file.


## Set up environment variables

Set up the following environment variables with the appropriate paths
used in your system.

| Name         | Value                  |
| ------------ | ---------------------- |
| `BOOST_ROOT` | C:\Programs\Boost\boost_1_81_0 |
| `GDAL_ROOT`  | C:\Programs\GDAL\3.6.2 |
| `GDAL_BIN`   | C:\Programs\GDAL\3.6.2\bin;C:\Programs\GDAL\3.6.2\bin\gdal\apps |
| `GDAL_DATA`  | C:\Programs\GDAL\3.6.2\bin\gdal-data |
| `GDAL_DRIVER_PATH` | C:\Programs\GDAL\3.6.2\bin\gdal\plugins |
| `PROJ_LIB`   | C:\Programs\GDAL\3.6.2\bin\proj6\share |
| `OpenCV_DIR` | C:\Programs\OpenCV\build |
| `MPI_HOME`   | C:\Program Files (x86)\Microsoft SDKs\MPI |

*Note:* for newer GDAL versions 3.8+, there is a `proj9` folder instead of `proj6`, which should be used.

Also add the GDAL and the OpenCV binary directories to the `PATH` environment variable, as these will be linked dynamically:
 - add the paths in the `GDAL_BIN` variable;
 - add the OpenCV binary directory (e.g. `C:\Programs\OpenCV\build\x64\vc16\bin`).


## Build *PointCloudTools* from command Line

Launch a *Developer Command Prompt for VS* and generate the build environment with CMake:
```batch
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=..\install
```

Build the solution and optionally install the binaries to the specified `install` destination.
```batch
msbuild CloudTools.sln
msbuild INSTALL.vcxproj
```


## Build *PointCloudTools* from Visual Studio

As a preliminary steps *(has to be done only once)*,
create a copy of the `CMakeSettings.json.sample` file with the
name `CMakeSettings.json`.
This file contains the prepared build configurations which will be loaded by Visual Studio for the *Ninja* generator used by CMake.

Then simply open the folder of the project in VS, as Visual Studio has integrated CMake support.
Build steps in Visual Studio:
 1. Select the `x64-Debug` or `x64-Release` build configuration in the top menu bar.  
*Remark:* You may use the *x86* versions if you have installed the 32 bit dependencies. Note however, that 32 bit processes have limited memory access around 3GB, which can be an issue when processing larger files with *PointCloudTools*.
 2. CMake should be executed automatically, but you can enforce to run it with the *Project -> Delete Cache and Reconfigure* option.
 3. Build the project with the *Build -> Build All* menu option.
 4. Install the project to the default `install` folder with the *Build -> Install CloudTools* option.
