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

# Set this if gcc-arm-none-eabi is not in path
#set(COMPILER_PREFIX /opt/gcc-arm-none-eabi/bin/)

# Set the compiler (must be prior to project setup)
include(${BASE_LOCATION}/toolchain/arm-gcc.cmake)

##### Project Setup #####
# Set build

# Note that "Release" has different flags.
# See efm32-base/toolchain/efm32-base.cmake
set(CMAKE_BUILD_TYPE Release)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE DEBUG)
endif ()

# for clock_gettime():
set(MY_C_FLAGS "${MY_C_FLAGS} -D__unix__")
set(MY_C_FLAGS "${MY_C_FLAGS} -D_POSIX_TIMERS")

set(MY_C_FLAGS "${MY_C_FLAGS} -fdiagnostics-color")

set(MY_C_FLAGS "${MY_C_FLAGS} -O2")

set(MY_C_FLAGS "${MY_C_FLAGS} -Wall")
set(MY_C_FLAGS "${MY_C_FLAGS} -Werror")
set(MY_C_FLAGS "${MY_C_FLAGS} -Wno-error=cpp")
set(MY_C_FLAGS "${MY_C_FLAGS} -Wno-error=array-bounds")
set(MY_C_FLAGS "${MY_C_FLAGS} -Wno-error=discarded-qualifiers")
set(MY_C_FLAGS "${MY_C_FLAGS} -Wno-error=missing-field-initializers")

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

##### Modules #####

# Libraries can be added to the LIBS variable
# or manually included here.

# Add base libs (emlib, CMSIS, device files)
include(${BASE_LOCATION}/toolchain/efm32-base.cmake)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${MY_C_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${MY_C_FLAGS}")

# Configure project and languages
project(space-ham C ASM)


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

# Generate executable and link
add_executable(${PROJECT_NAME}
    main.c
	serial.c
	strutil.c
	linklist.c
	iadc.c
	pwm.c
	rotor.c
	flash.c
	systick.c
	rtcc.c
	pid.c
	sat.c
	i2c.c
	stars.c

	i2c/rtc-ds3231.c
	i2c/ads111x.c

	sgp4sdp4/sgp4sdp4.c
	sgp4sdp4/sgp_in.c
	sgp4sdp4/sgp_math.c
	sgp4sdp4/sgp_obs.c
	sgp4sdp4/sgp_time.c
	sgp4sdp4/solar.c

	astronomy/source/c/astronomy.c

	xmodem/xmodem.c

	fatfs-efr32.c
	fatfs-xmodem.c
	fatfs/ff.c
	)

# Add project headers
target_include_directories(${PROJECT_NAME} PRIVATE
       ${BASE_LOCATION}/include
       include
       sgp4sdp4
       astronomy/source/c
       xmodem
       fatfs
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
