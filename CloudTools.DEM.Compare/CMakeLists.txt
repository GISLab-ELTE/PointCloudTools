include_directories(../)

add_executable(dem_compare
	main.cpp
	Comparison.h)
target_link_libraries(dem_compare
	dem common
	${GDAL_LIBRARY})

install(TARGETS dem_compare
	DESTINATION ${CMAKE_INSTALL_PREFIX})
