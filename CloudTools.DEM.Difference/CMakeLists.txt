include_directories(../)

add_executable(dem_diff
	main.cpp)
target_link_libraries(dem_diff
	dem common
	${GDAL_LIBRARY})

install(TARGETS dem_diff
	DESTINATION ${CMAKE_INSTALL_PREFIX})
