include_directories(../)

add_executable(ahn_vegetation
	main.cpp
	NoiseFilter.cpp NoiseFilter.h)

target_link_libraries(ahn_vegetation
	dem
	common)

install(TARGETS ahn_vegetation
		DESTINATION ${CMAKE_INSTALL_PREFIX})