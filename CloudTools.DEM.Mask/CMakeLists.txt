include_directories(../)

add_executable(dem_mask
	main.cpp)
target_link_libraries(dem_mask
	dem common)

install(TARGETS dem_mask
	DESTINATION ${CMAKE_INSTALL_PREFIX})
