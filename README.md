# space-ham: Track Satellites

The reason I started ham radio was because I wanted to contact the International Space Station.

Part of doing that is creating an antenna rotor that can follow a satellite in the sky, and of course there is much more to it than that! This program is made to control for tracking satellites using the Silicon Labs EFR32MG21. If you would like to follow the entire project, then please visit my youtube channel for more information including demonstration videos:
- [Zeke, KJ7NLL's YouTube Channel](https://www.youtube.com/channel/UCbcLQGEDcnNrOn7Hp0hvtUA)
- [Demonstration: Tracking The International Space Station with LEGO's!](https://www.youtube.com/channel/UCbcLQGEDcnNrOn7Hp0hvtUA)

## About the Program

### Acknowledgments
- This program uses Ryan Kurte's awesome [efm32-base Github repository](https://github.com/ryankurte/efm32-base) that containes the Silicon Labs SDK to compile from the command-line using `cmake`. It also means we can write all of the code using `vim` on the command line!
- We are using the [sgp4sdp4 library](https://github.com/KJ7LNW/sgp4sdp4) that was ported from Pascal to C by [Neoklis, 5B4AZ](http://www.5b4az.org) to track orbiting satellites
- The pid controller that sets the PWM duty_cycle for each rotor to track satellites was written by [Philip Salmony](https://github.com/pms67/PID)
- [FatFs by ChaN](http://elm-chan.org/fsw/ff/00index_e.html)
- Celestial body tracking using [Astronomy Engine](https://github.com/cosinekitty/astronomy)

### Features

This program implements the followiong efr32 features for the EFR32MG21 Microcontroller:
- Multiple PWM outputs using the internal timers
- UART console (USART0)
- Analog to digital conversion to measure rotor position (IADC)
- Read/write of on-board NAND
- FAT filesystem running from onboard NAND
- Systick interrupts for frequent functon execution
- Realtime clock counter for keeping time
- I2C reading from the [DS3231 RTC](https://datasheets.maximintegrated.com/en/ds/DS3231.pdf)

Not only does this project provide a way to control rotors, it also serves a working example for implementing these features.
