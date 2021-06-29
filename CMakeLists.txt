# Base EFM32 CMake file
#
# This can be used as is as a project base, or by adding the efm32-base
# repository as a submodule to another project, copying this CMakeLists file
# to the top level directory, and updating the BASE_LOCATION variable to reflect this
# change
#
## Copyright (c) 2016 Ryan Kurte
# This file is covered under the MIT license available at: https://opensource.org/licenses/MIT

###### Project Environment #####

# Set minimum CMake version
cmake_minimum_required(VERSION 2.8.4)

# Optional verbose mode, uncomment or pass in -DCMAKE_VERBOSE_MAKEFILE=ON
# set(CMAKE_VERBOSE_MAKEFILE ON)

set(BASE_LOCATION efm32-base)

set(COMPILER_PREFIX /opt/gcc-arm-none-eabi/bin/)

# Set the compiler (must be prior to project setup)
include(${BASE_LOCATION}/toolchain/arm-gcc.cmake)

##### Project Setup #####

# Configure project and languages
project(space-ham C CXX ASM)

# ${DEVICE} sets the target specific model name
if (NOT DEVICE)
    set(DEVICE EFR32MG21A020F1024IM32)
    # set(DEVICE BGM13P22F512GA)
    set(BOARD BRD4180A)
    # set(BLE_LIB EFR32BG13P)
    #set(FLASH_ORIGIN 0x0)
endif ()


# ${JLINK_DEVICE} device model setting specifically for JLink. (Defaults to ${DEVICE} when not set)
# set(JLINK_DEVICE EFM32TG11BXXXF128)

# When ${JLINK_SERVER_IP} is set, JLink will try to connect over IP to a JLink server
# set(JLINK_SERVER_IP 127.0.0.1)

# Set build
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE DEBUG)
endif ()

##### Modules #####

# Libraries can be added to the LIBS variable
# or manually included here.

# Add base libs (emlib, CMSIS, device files)
include(${BASE_LOCATION}/toolchain/efm32-base.cmake)

# Enable protocols / BLE if desired
# Note this will overwrite the default linker files / may not behave in the manner you expect
#set(USE_PROTOCOL 1)

add_subdirectory(${BASE_LOCATION}/cmsis)
add_subdirectory(${BASE_LOCATION}/emlib)
add_subdirectory(${BASE_LOCATION}/device)
add_subdirectory(${BASE_LOCATION}/protocol)
add_subdirectory(${BASE_LOCATION}/hardware)

# This is normally set in efm32-base.cmake, but libraries may modify it so set
# it after libraries/subdirectories have been added
set(CMAKE_EXE_LINKER_FLAGS "${COMMON_DEFINITIONS} -Xlinker -T${LINKER_SCRIPT} -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections -Wl,-v -u _printf_float")

##### Files #####

# Add project headers
#include_directories(bsp)
include_directories(${BASE_LOCATION}/include
	include
	)

# Generate executable and link
add_executable(${PROJECT_NAME}
	#${BASE_LOCATION}/hardware/kit/common/drivers/retargetio.c
	#${BASE_LOCATION}/hardware/kit/common/drivers/retargetserial.c
	#main_gpio_conf_s2.c
	#main_gpio_periph_s2.c
        main.c
	serial.c
	strutil.c
	linklist.c
	iadc.c
	pwm.c
	rotor.c
	flash.c
	systick.c
	pid.c
	)

efm32_configure_linker_addresses(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} ${LIBS} m emlib cmsis device)

# Link optional libraries if available
if (${HAS_HARDWARE})
    target_link_libraries(${PROJECT_NAME} hardware)
endif ()

if (${HAS_PROTOCOL})
    target_link_libraries(${PROJECT_NAME} protocol)
endif ()

##### Post build #####

# Add post build commands
include(${BASE_LOCATION}/toolchain/post-build.cmake)

# Add JLink commands
include(${BASE_LOCATION}/toolchain/jlink.cmake)

##### CMake debug prints #####
if (CMAKE_VERBOSE_MAKEFILE)
    print_debug_info()
endif()
