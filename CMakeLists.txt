cmake_minimum_required(VERSION 3.20)

if(IDF_VERSION_MAJOR)
	idf_component_register(
		SRCS
			"src/ads131m0x.c"
		INCLUDE_DIRS "include"
	)
elseif(ZEPHYR_BASE)
	zephyr_library()
	zephyr_library_sources(
		src/ads131m0x.c
	)
	zephyr_include_directories(include)
else()
	add_library(ads131m0x STATIC
		src/ads131m0x.c
	)
	target_include_directories(ads131m0x PUBLIC include)
endif()
