CloudTools
============

Massive airborne laser altimetry (ALS) point cloud and digital elevation model (DEM) processing library.  
Contains the **AHN Building Change Detector**, a toolset for change detection of the built-up area based on the study dataset: [AHN - Actueel Hoogtebestand Nederland](http://www.ahn.nl/).


Dependencies
------------
- [Boost Library](https://www.boost.org/), >=1.60 *(mandatory)*
- [GDAL - Geospatial Data Abstraction Library](http://www.gdal.org/)<sup>1</sup>, >=2.1 *(mandatory)*
- MPI - Message Passing Interface<sup>2</sup> *(optional)*
  - Windows: [MS-MPI](https://msdn.microsoft.com/en-us/library/bb524831), >=7.1
  - Linux: [Open MPI](https://www.open-mpi.org/), >=1.10

For Windows operating systems binary releases are available to simply install the above defined libraries.  
For Linux operating systems installing from the standard (or other) package repository is a convenient solution if the available versions meet the requirements. Otherwise building the dependencies from source is recommended.

<sup>1</sup> GDAL must be compiled with [GEOS](https://trac.osgeo.org/geos/) support enabled.  
<sup>2</sup> MPI dependency is required only for the `AHN.Buildings.MPI` project to evaluate in a HPC environment.


Structure
------------

The repository in consisted of 9 projects altogether.
- **CloudTools.DEM:** Algorithms on DEM transformation and calculation.
- **CloudTools.DEM.Difference:** Compares multi-temporal DEMs of same area to retrieve differences.
- **CloudTools.DEM.Mask:** Transforms a vector filter mask into a raster filter mask and/or applies the latter on a DEM.
- **CloudTools.Common:** Reporting and I/O management.
- **AHN.Buildings:** Compares an AHN-2 and AHN-3 tile pair and filters out changes in buildings.
- **AHN.Buildings.Parallel:** Compares pairs of AHN-2 and AHN-3 tiles parallely and filters out changes in buildings.
- **AHN.Buildings.MPI:** Compares pairs of AHN-2 and AHN-3 tiles parallely (through MPI) and filters out changes in buildings.
- **AHN.Buildings.Aggregate:** Computes aggregative change of volume for administrative units.
- **AHN.Buildings.Verify:** Verifies detected building changes against reference files.


How to build
------------

The project was built and tested on the following operating systems:
- Windows 10
- Windows 7
- Ubuntu Linux 16.04 LTS

The repository utilizes the [CMake](https://cmake.org/) cross-platform build system. To generate the build environment, run CMake:
```bash
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=<install_path>
```

The following table contains a few CMake variables which might be relevant
during compilation.

| Variable | Meaning |
| -------- | ------- |
| `CMAKE_INSTALL_PREFIX` | Install directory. For more information see: https://cmake.org/cmake/help/v3.5/variable/CMAKE_INSTALL_PREFIX.html |
| `CMAKE_BUILD_TYPE` | Specifies the build type on single-configuration generators (e.g. *Unix Makefiles*). Possible values are empty, **Debug**, **Release**, **RelWithDebInfo** and **MinSizeRel**. For more information see: https://cmake.org/cmake/help/v3.5/variable/CMAKE_BUILD_TYPE.html |

### On Windows

Using CMake with the *MSVC generator* it will construct a Visual Studio solution which can be used to build the projects. Open a *Developer Command Prompt for Visual Studio* to build the solution and optionally install the binaries to the specified destination:
```batch
msbuild CloudTools.sln
msbuild INSTALL.vcxproj
```

You may specify the `/property:Configuration=<value>` flag to set the project configuration. Supported values are **Debug** and **Release**.

The project supports Visual Studio's CMake integration, you may create a `CMakeSettings.json` file based on the given sample (`CMakeSettings.json.sample`) and use the multi-configuration *Ninja* generator to compile against x86 or x64 architectures.

### On Linux

Using CMake with the *Unix Makefiles* generator it will construct [GNU Makefiles](https://www.gnu.org/software/make/) as a build system. Then a simple `make` command is sufficient for compilation.  
Specify one of the above mentioned project names in section [Structure](#structure) as the build target to only build a subset of the projects, e.g.:
~~~bash
make
make AHN.Buildings.Parallel
~~~
*Note:* you may add the `-j<N>` flag to compile on multiple threads (where `<N>` is the number of threads).  

How to use
------------

On build of repository, the following executables will be available:
```
dem_diff
dem_mask
ahn_buildings_sim
ahn_buildings_par
ahn_buildings_agg
ahn_buildings_ver
ahn_buildings_mpi
```
Get usage information and available arguments with the `-h` flag.

*Note:* since the GDAL library is linked dynamically, some environment variables (`GDAL_BIN`, `GDAL_DATA` and `GDAL_DRIVER_PATH`) must be configured to execute the above binaries. Set them correctly in your OS account or in the active shell.  
On Windows you may create a `Shell.config.cmd` file based on the given sample (`Shell.config.cmd.sample`) and run the `Shell.bat` preconfigured environment at the installation location.
