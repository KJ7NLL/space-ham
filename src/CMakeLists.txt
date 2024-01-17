add_library(space-ham-src)

if (DEFINED USE_EFM32_BASE)
	target_compile_options(space-ham-src PRIVATE
		-D__unix__
		-D__EFR32__
		)
	message("USING EFM32: ${EFM32_BASE_LOCATION}")
	include(${EFM32_BASE_LOCATION}/toolchain/efm32-base.cmake)
	set(CMAKE_BUILD_TYPE Release)
	target_link_libraries(space-ham-src m emlib cmsis device)
endif()

# Generate executable and link
target_sources(space-ham-src PRIVATE
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
		i2c/qmc5883l.c

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
target_include_directories(space-ham-src PUBLIC
       include
       sgp4sdp4
       astronomy/source/c
       xmodem
       fatfs
       )

target_link_libraries(space-ham-src m)
