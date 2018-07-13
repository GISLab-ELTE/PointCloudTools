include_directories(../)

add_executable(ahn_buildings_agg
	main.cpp
	Region.h)
target_link_libraries(ahn_buildings_agg
	dem common)

install(TARGETS ahn_buildings_agg
	DESTINATION ${CMAKE_INSTALL_PREFIX})
