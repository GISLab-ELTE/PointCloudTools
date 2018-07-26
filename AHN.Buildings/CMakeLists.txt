include_directories(../)

add_library(ahn_buildings
	BuildingExtraction.cpp BuildingExtraction.h
	BuildingFilter.cpp BuildingFilter.h
	Comparison.cpp Comparison.h
	IOMode.cpp IOMode.h
	Process.cpp Process.h
	Result.cpp Result.h)

add_executable(ahn_buildings_sim
	main.cpp)
target_link_libraries(ahn_buildings_sim
	ahn_buildings
	dem common)

install(TARGETS ahn_buildings_sim
	DESTINATION ${CMAKE_INSTALL_PREFIX})
