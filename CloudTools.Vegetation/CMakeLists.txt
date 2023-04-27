include_directories(../)

add_executable(vegetation
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

target_link_libraries(vegetation
	dem
	common
	Threads::Threads)

install(TARGETS vegetation
	DESTINATION ${CMAKE_INSTALL_PREFIX})

# ahn_vegetation -> vegetation symlink for backward compatibility
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ./vegetation ${CMAKE_INSTALL_PREFIX}/ahn_vegetation)")