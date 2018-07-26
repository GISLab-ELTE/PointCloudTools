include_directories(../)
include_directories(${MPI_CXX_INCLUDE_PATH})

link_libraries(${MPI_CXX_LIBRARIES})

add_executable(ahn_buildings_mpi
	main.cpp)
target_link_libraries(ahn_buildings_mpi
	ahn_buildings
	dem common)

install(TARGETS ahn_buildings_mpi
	DESTINATION ${CMAKE_INSTALL_PREFIX})
