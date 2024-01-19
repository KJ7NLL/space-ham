#ifndef __PLATFORM_H

#define __PLATFORM_H

#include <stdint.h>

#ifdef __EFR32__

#include "em_chip.h"
#include "em_cmu.h"
#include "em_device.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_i2c.h"
#include "em_iadc.h"
#include "em_msc.h"
#include "em_rtcc.h"
#include "em_timer.h"
#include "em_usart.h"

#define HAVE_IADC 1
#define HAVE_I2C 1
inline static void platform_sleep()
{
	EMU_EnterEM1();
}

#else

#define FLASH_PAGE_SIZE 4096
typedef void* TIMER_TypeDef;
typedef int I2C_TransferReturn_TypeDef;

inline static void platform_sleep()
{
}

#endif
#endif
