include_directories(../)

add_library(ahn_buildings_lib
	BuildingExtraction.cpp BuildingExtraction.h
	BuildingFilter.cpp BuildingFilter.h
	ClusterFilter.cpp ClusterFilter.h
	Comparison.cpp Comparison.h
	IOMode.cpp IOMode.h
	MajorityFilter.cpp MajorityFilter.h
	MorphologyFilter.cpp MorphologyFilter.h
	NoiseFilter.cpp NoiseFilter.h
	Process.cpp Process.h
	Result.cpp Result.h)

add_executable(ahn_buildings
	main.cpp)
target_link_libraries(ahn_buildings
	ahn_buildings_lib
	dem common)

install(TARGETS ahn_buildings
	DESTINATION ${CMAKE_INSTALL_PREFIX})
