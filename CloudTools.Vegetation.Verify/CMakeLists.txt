include_directories(../)

add_executable(vegetation_ver
	main.cpp)
target_link_libraries(vegetation_ver
	dem common)

install(TARGETS vegetation_ver
	DESTINATION ${CMAKE_INSTALL_PREFIX})
