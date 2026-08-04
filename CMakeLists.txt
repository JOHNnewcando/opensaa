cmake_minimum_required(VERSION 2.8)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME})


set(CMAKE_SHARED_LIBRARY_PREFIX "../")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".asi")


option(MAP "Generate .map file [ON/OFF]" OFF)
option(UPX "Execute UPX after build [ON/OFF]" ON)
option(EXPORT_LIST "Use list of export symbols [ON/OFF]" ON)


ADD_DEFINITIONS(-DWIN32=1)



find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()



aux_source_directory(
	${CMAKE_CURRENT_SOURCE_DIR}
	${PROJECT_NAME}_LIST
)



if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")

	add_definitions("-ffunction-sections -pipe -w")

	set(CMAKE_SHARED_LINKER_FLAGS
	"-Wl,--strip-all -Wl,--gc-sections -ffast-math -fno-stack-protector"
	)


elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")

	add_definitions("-ffunction-sections -fexec-charset=CP1251 -pipe -w")

	set(CMAKE_SHARED_LINKER_FLAGS
	"-Wl,--strip-all -Wl,--gc-sections -ffast-math"
	)

endif()



if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU"
 OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")


	if(EXPORT_LIST)

		set(
		CMAKE_SHARED_LINKER_FLAGS
		"${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script='${CMAKE_CURRENT_SOURCE_DIR}/symbols.export'"
		)

	endif()



	if(MAP)

		set(
		CMAKE_SHARED_LINKER_FLAGS
		"${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Map=${PROJECT_NAME}.map"
		)

	endif()

endif()



add_library(
	${PROJECT_NAME}
	SHARED
	${${PROJECT_NAME}_LIST}
)



# ==========================
# Windows libraries
# ==========================

target_link_libraries(
	${PROJECT_NAME}

	psapi
	version
)



set_target_properties(
	${PROJECT_NAME}

	PROPERTIES

	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)



if (UPX)

	find_program(UPX_BIN upx)

	if(UPX_BIN)

		add_custom_command(
			TARGET ${PROJECT_NAME}
			POST_BUILD
			COMMAND ${UPX_BIN} -9 $<TARGET_FILE:${PROJECT_NAME}>
		)

	else()

		message("UPX enabled, but not found in $PATH")

	endif()

endif()
