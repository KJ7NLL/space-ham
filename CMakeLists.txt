# Base EFM32 CMake file
#
# This can be used as is as a project base, or by adding the efm32-base
# repository as a submodule to another project, copying this CMakeLists file
# to the top level directory, and updating the EFM32_BASE_LOCATION variable to reflect this
# change
#
## Copyright (c) 2016 Ryan Kurte
# This file is covered under the MIT license available at: https://opensource.org/licenses/MIT

###### Project Environment #####

# Set minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Optional verbose mode, uncomment or pass in -DCMAKE_VERBOSE_MAKEFILE=ON
# set(CMAKE_VERBOSE_MAKEFILE ON)

# Uncomment USE_EFM32_BASE to enable building on Silacon Labs EFM32/EFR32 devices.
# You will need to specify your board and device model below.
#set(USE_EFM32_BASE 1)

# Uncomment USE_ESP32 to enable building on Espressif ESP32 devices
# To build on ESP32, run `idf.py set-target esp32-c6 && idf.py flash`
set(USE_ESP32 ${ESP_PLATFORM})

# Leave both USE_EFM32_BASE and USE_ESP32 disabled to build on Linux/UNIX.

if (DEFINED USE_EFM32_BASE)
	# Set this if gcc-arm-none-eabi is not in path
	#set(COMPILER_PREFIX /opt/gcc-arm-none-eabi/bin/)

	# ${DEVICE} sets the target specific model name
	set(BOARD BRD4180A)
	set(DEVICE EFR32MG21A020F1024IM32)

	# Set the compiler (must be prior to project setup)
	set(EFM32_BASE_LOCATION ${CMAKE_SOURCE_DIR}/efm32-base)
	include(${EFM32_BASE_LOCATION}/toolchain/arm-gcc.cmake)
endif ()

##### Project Setup #####
# Set build

# Configure project and languages

if (DEFINED USE_ESP32)
	include($ENV{IDF_PATH}/tools/cmake/project.cmake)
	add_compile_options(-fdiagnostics-color=always)
	add_compile_definitions(
		ASTRONOMY_ENGINE_WHOLE_SECOND
		__ESP32__
		timegm=mktime
	)
	project(space-ham)
else ()
	project(space-ham C ASM)

	if (NOT CMAKE_BUILD_TYPE)
	    set(CMAKE_BUILD_TYPE DEBUG)
	endif ()

	list(APPEND MY_C_FLAGS "-fdiagnostics-color")

	list(APPEND MY_C_FLAGS "-Wall")
	list(APPEND MY_C_FLAGS "-Werror")
	list(APPEND MY_C_FLAGS "-Wno-error=cpp")
	list(APPEND MY_C_FLAGS "-Wno-error=array-bounds")
	list(APPEND MY_C_FLAGS "-Wno-error=discarded-qualifiers")
	list(APPEND MY_C_FLAGS "-Wno-error=missing-field-initializers")

	# Generate executable and link
	add_executable(${PROJECT_NAME} /dev/null)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)

	add_subdirectory(main)
	target_link_libraries(${PROJECT_NAME} space-ham-src)

endif ()

if (DEFINED USE_EFM32_BASE)
	# Note that "Release" has different flags.  See efm32-base/toolchain/efm32-base.cmake.
	#
	# For EFM32, we only use a "Release" version because its smaller and
	# debug symbols aren't that helpful because gdb doesn't work well
	# remotely.
	set(CMAKE_BUILD_TYPE Release)

	include(${EFM32_BASE_LOCATION}/toolchain/efm32-base.cmake)

	# for clock_gettime():
	target_compile_options(${PROJECT_NAME} PUBLIC ${MY_C_FLAGS})

	# Add base libs (emlib, CMSIS, device files)
	add_subdirectory(${EFM32_BASE_LOCATION}/cmsis)
	add_subdirectory(${EFM32_BASE_LOCATION}/emlib)
	add_subdirectory(${EFM32_BASE_LOCATION}/device)

	target_link_libraries(${PROJECT_NAME} ${LIBS} m emlib cmsis device)

	# This is normally set in efm32-base.cmake, but libraries may modify it
	# so set it after libraries/subdirectories have been added
	target_link_options(${PROJECT_NAME} PUBLIC
		-Xlinker
		-T${LINKER_SCRIPT}
		-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map
		-Wl,--gc-sections
		-Wl,-v
		-u _printf_float
		)

	efm32_configure_linker_addresses(${PROJECT_NAME})

	##### Post build #####

	# Add post build commands
	include(${EFM32_BASE_LOCATION}/toolchain/post-build.cmake)

	# Add JLink commands
	include(${EFM32_BASE_LOCATION}/toolchain/jlink.cmake)

endif ()

##### CMake debug prints #####
#if (CMAKE_VERBOSE_MAKEFILE)
 #   print_debug_info()
#endif ()
