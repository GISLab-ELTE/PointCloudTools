#pragma once

#include <string>

#include <boost/filesystem.hpp>

#include "IOMode.h"

namespace fs = boost::filesystem;

namespace AHN
{
	// TODO: refactor to OOP
	void buildingFilter(std::string tileName, fs::path ahn2Dir, fs::path ahn3Dir, fs::path outputDir, IOMode mode, bool debug, bool quiet);
}