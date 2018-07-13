include_directories(../)

add_executable(dem_filter
	main.cpp)
target_link_libraries(dem_filter
	dem common)

install(TARGETS dem_filter
	DESTINATION ${CMAKE_INSTALL_PREFIX})
