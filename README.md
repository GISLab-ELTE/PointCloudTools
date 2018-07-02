CloudTools
============

Massive airborne laser altimetry (ALS) point cloud and digital elevation model (DEM) processing library.  
Contains the **AHN Building Change Detector**, a toolset for change detection of the built-up area based on the study dataset: [AHN - Actueel Hoogtebestand Nederland](http://www.ahn.nl/).


Dependencies
------------
- [Boost Library](https://www.boost.org/), >=1.60 *(mandatory)*
- [GDAL - Geospatial Data Abstraction Library](http://www.gdal.org/), >=2.1 *(mandatory)*
- MPI - Message Passing Interface *(optional)*
  - Windows: [MS-MPI](https://msdn.microsoft.com/en-us/library/bb524831), >=7.1
  - Linux: [Open MPI](https://www.open-mpi.org/), >=1.10

For Windows operating systems binary releases are available to simply install the above defined libraries.  
For Linux operating systems installing from the standard (or other) package repository is a convenient solution if the available versions meet the requirements. Otherwise building the dependencies from source is recommended.

The MPI dependency is required for the `AHN.Buildings.MPI` project to evaluate in a HPC environment.


Structure
------------

The repository in consisted of 3 solutions and 9 projects altogether.
- **CloudLib**: Library of general point cloud processing algorithms.
  - *CloubLib.DEM:* Algorithms on DEM transformation and calculation.
- **CloudTools**: General tools for point cloud processing and comparison.
  - *CloudTools.Common:* Reporting and I/O management.
  - *CloudTools.DEM.Compare:* Compares multi-temporal DEMs of same area to retrieve differences.
  - *CloudTools.DEM.Filter:* Transforms a vector filter into a raster filter and/or applies the latter on a DEM.
- **AHN**: Specific tools for processing the AHN dataset.
  - *AHN.Buildings:* Compares an AHN-2 and AHN-3 tile pair and filters out changes in buildings.
  - *AHN.Buildings.Parallel:* Compares pairs of AHN-2 and AHN-3 tiles parallely and filters out changes in buildings.
  - *AHN.Buildings.MPI:* Compares pairs of AHN-2 and AHN-3 tiles parallely (through MPI) and filters out changes in buildings.
  - *AHN.Buildings.Aggregate:* Computes aggregative change of volume for administrative units.
  - *AHN.Buildings.Verify:* Verifies detected building changes against reference files.


How to build
------------

The project was built and tested on the following operating systems:
- Windows 10
- Windows 7
- Ubuntu Linux 16.04 LTS


### On Windows

The repository contains the respective Visual Studio solution and project files which can be used to open and/or build the projects.

Before compiling the source, the include and library paths of the Boost and GDAL libraries must be configured in the `CloudLib.DEM.<arch>` and the `CloudTools.DEM.<arch>` property sheets (e.g. `CloudTools.DEM.x64`). You can edit them visually in Visual Studio through the *Property Manager* or by opening the respective files in any text editor (e.g. `CloudTools.DEM.x64.props`)

### On Linux

The repository contains the required [GNU Makefile](https://www.gnu.org/software/make/) for building.  
Create a `Makefile.config` file based on the given sample (`Makefile.config.sample`) and modify the flags and options as desired. Make sure that all libraries mentioned in the [Dependencies](#dependencies) section are available on the standard include and library paths or define them in the respective flags.

Then a simple `make` command is sufficient for compilation.  
Specify one of the above mentioned project or solution names in section [Structure](#structure) as the build target to only build a subset of the projects, e.g.:
~~~bash
make CloudTools
make AHN.Buildings.Parallel
~~~
*Note:* you may add the `-j<N>` flag to compile on multiple threads (where `<N>` is the number of threads).  

How to use
------------

On build of the *CloudTools* and *AHN* solutions, the following executables will be available:
```
dem_compare
dem_filter
ahn_buildings
ahn_buildings_par
ahn_buildings_agg
ahn_buildings_ver
ahn_buildings_mpi
```
Get usage information and available arguments with the `-h` flag.

*Note:* since the GDAL library is linked dynamically, some environment variables (`GDAL_BIN`, `GDAL_DATA` and `GDAL_DRIVER_PATH`) must be configured to execute the above binaries. Set them correctly in your OS account or (on Windows) create a `Shell.config.cmd` file based on the given sample (`Shell.config.cmd.sample`) and run the `Shell.bat` preconfigured environment.
