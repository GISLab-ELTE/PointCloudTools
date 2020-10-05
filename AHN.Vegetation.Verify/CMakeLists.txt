include_directories(../)

add_executable(ahn_vegetation_ver
	main.cpp)
target_link_libraries(ahn_vegetation_ver
	dem common)

install(TARGETS ahn_vegetation_ver
	DESTINATION ${CMAKE_INSTALL_PREFIX})
