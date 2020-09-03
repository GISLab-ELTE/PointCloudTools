cmake_minimum_required(VERSION 3.5)
project(CloudTools)

# Set a default build type if none was specified
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE
        STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # warning level 4
    add_compile_options(/W4)
else()
    # add most warnings
    add_compile_options(-pedantic -Wall -Wno-unknown-pragmas -Wno-reorder)
endif()

find_package(Boost 1.58 REQUIRED COMPONENTS filesystem program_options regex)
find_package(GDAL 2.1 REQUIRED)
find_package(Threads REQUIRED)
find_package(MPI)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${GDAL_INCLUDE_DIR})

if(MSVC)
    link_directories(${Boost_LIBRARY_DIRS})
else()
    link_libraries(${Boost_LIBRARIES})
endif()
link_libraries(${GDAL_LIBRARY})

add_subdirectory(CloudTools.Common)
add_subdirectory(CloudTools.DEM)
add_subdirectory(CloudTools.DEM.Difference)
add_subdirectory(CloudTools.DEM.Mask)
add_subdirectory(AHN.Buildings)
add_subdirectory(AHN.Buildings.Parallel)
add_subdirectory(AHN.Buildings.Aggregate)
add_subdirectory(AHN.Buildings.Verify)
add_subdirectory(AHN.Vegetation)

if(MPI_CXX_FOUND)
    add_subdirectory(AHN.Buildings.MPI)
else()
	message(WARNING "MPI not found, AHN.Buildings.MPI will not be built.")
endif()

if(WIN32)
    install(FILES Shell.bat
        DESTINATION ${CMAKE_INSTALL_PREFIX})
    if(EXISTS ${CMAKE_SOURCE_DIR}/Shell.config.cmd)
        install(FILES Shell.config.cmd
            DESTINATION ${CMAKE_INSTALL_PREFIX})
    else()
        message(WARNING "Create Shell.config.cmd config file from Shell.config.cmd.sample!")
    endif()
endif()
