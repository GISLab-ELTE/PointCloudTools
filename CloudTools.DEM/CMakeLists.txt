include_directories(../)

add_library(dem
	Calculation.cpp Calculation.h
	Creation.cpp Creation.h
	Transformation.cpp Transformation.h
	Color.cpp Color.h
	Helper.cpp Helper.h
	Metadata.cpp Metadata.h
	Rasterize.cpp Rasterize.h
	ClusterMap.cpp ClusterMap.h
	Window.hpp
	SweepLineCalculation.hpp
	SweepLineTransformation.hpp
	DatasetCalculation.hpp
	DatasetTransformation.hpp
	Filters/ClusterFilter.hpp
	Filters/MajorityFilter.hpp
	Filters/MorphologyFilter.hpp
	Filters/NoiseFilter.hpp
	Comparers/Difference.hpp
	Algorithms/HierachicalClustering.hpp
	Algorithms/MatrixTransformation.cpp Algorithms/MatrixTransformation.h)
