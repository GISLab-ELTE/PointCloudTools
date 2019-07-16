include_directories(../)

add_executable(ahn_vegetation
	main.cpp
	NoiseFilter.cpp NoiseFilter.h
	TreeCrownSegmentation.cpp TreeCrownSegmentation.h
	HausdorffDistance.cpp HausdorffDistance.h
	MorphologyClusterFilter.h MorphologyClusterFilter.cpp
	CentroidDistance.cpp CentroidDistance.h
	InterpolateNoData.hpp)

target_link_libraries(ahn_vegetation
	dem
	common
	Threads::Threads)

install(TARGETS ahn_vegetation
		DESTINATION ${CMAKE_INSTALL_PREFIX})