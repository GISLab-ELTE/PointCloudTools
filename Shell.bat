@echo off
REM CloudTools Environment

:: Load configuration
if exist "%~dp0\Shell.config.cmd" (
  call "%~dp0\Shell.config.cmd"
) else (
  echo Create Shell.config.cmd config file from Shell.config.cmd.sample!
  exit /b
)
set PATH=%CLOUDTOOLS_BIN%;%GDAL_BIN%;%PATH%

:: Check GDAL/OGR
where gdalinfo >nul 2>&1
if %ERRORLEVEL% neq 0 (
  echo GDAL/OGR tools not found.
  exit /b
)

:: Check CloudTools
where dem_compare >nul 2>&1
if %ERRORLEVEL% neq 0 (
  echo CloudTools not found.
  exit /b
)

echo === CloudTools Environment ===
echo.

@if [%1]==[] (
  echo Available programs:
  echo  dem_compare
  echo  dem_filter
  echo  ahn_buildings
  echo  ahn_buildings_par
  echo  ahn_buildings_agg
  echo  ahn_buildings_ver
  echo  ahn_buildings_mpi
  echo.
  cmd.exe /k ""
) else (
  cmd /c "%*"
)