include_directories(../)

add_executable(ahn_buildings_par
	main.cpp)
target_link_libraries(ahn_buildings_par
	ahn_buildings_lib
	dem common
	Threads::Threads)

install(TARGETS ahn_buildings_par
	DESTINATION ${CMAKE_INSTALL_PREFIX})
