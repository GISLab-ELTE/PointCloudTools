add_library(dem
	Calculation.cpp Calculation.h
	Color.cpp Color.h
	Helper.cpp Helper.h
	Metadata.cpp Metadata.h
	Operation.cpp Operation.h
	Rasterize.cpp Rasterize.h
	SweepLineCalculation.hpp
	SweepLineTransformation.hpp
	Transformation.cpp Transformation.h
	Window.hpp
	Filters/ClusterFilter.hpp
	Filters/MajorityFilter.hpp
	Filters/MorphologyFilter.hpp
	Filters/NoiseFilter.hpp
	Comparers/Difference.hpp)
