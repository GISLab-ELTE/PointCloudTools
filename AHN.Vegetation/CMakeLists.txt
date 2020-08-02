include_directories(../)

add_executable(ahn_vegetation
	main.cpp
	NoiseFilter.cpp NoiseFilter.h
	TreeCrownSegmentation.cpp TreeCrownSegmentation.h
	HausdorffDistance.cpp HausdorffDistance.h
	MorphologyClusterFilter.cpp MorphologyClusterFilter.h
	CentroidDistance.cpp CentroidDistance.h
	InterpolateNoData.cpp InterpolateNoData.h
	EliminateNonTrees.cpp EliminateNonTrees.h
	VolumeDifference.cpp VolumeDifference.h
	DistanceCalculation.h
	HeightDifference.h HeightDifference.cpp
	PreProcess.cpp PreProcess.h
	PostProcess.cpp PostProcess.h)

target_link_libraries(ahn_vegetation
	dem
	common
	Threads::Threads)

install(TARGETS ahn_vegetation
	DESTINATION ${CMAKE_INSTALL_PREFIX})