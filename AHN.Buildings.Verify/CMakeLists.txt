include_directories(../)

add_executable(ahn_buildings_ver
	main.cpp
	Coverage.h)
target_link_libraries(ahn_buildings_ver
	dem common)

install(TARGETS ahn_buildings_ver
	DESTINATION ${CMAKE_INSTALL_PREFIX})
