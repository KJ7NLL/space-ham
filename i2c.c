#include "em_device.h"
#include "em_chip.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_i2c.h"

#include "i2c.h"

#define I2C_PORT gpioPortD
#define I2C_SDA 2
#define I2C_SCL 3

void initI2C()
{
	CMU_ClockEnable(cmuClock_I2C0, true);

	// Using default settings
	I2C_Init_TypeDef i2cInit = I2C_INIT_DEFAULT;

	// Route GPIO pins to I2C module
	GPIO->I2CROUTE[0].SDAROUTE =
		(GPIO->I2CROUTE[0].SDAROUTE & ~_GPIO_I2C_SDAROUTE_MASK) |
		((I2C_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT) |
		 (I2C_SDA << _GPIO_I2C_SDAROUTE_PIN_SHIFT));
	GPIO->I2CROUTE[0].SCLROUTE =
		(GPIO->I2CROUTE[0].SCLROUTE & ~_GPIO_I2C_SCLROUTE_MASK) |
		((I2C_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT) |
		 (I2C_SCL << _GPIO_I2C_SCLROUTE_PIN_SHIFT));
	GPIO->I2CROUTE[0].ROUTEEN =
		GPIO_I2C_ROUTEEN_SDAPEN | GPIO_I2C_ROUTEEN_SCLPEN;

	I2C_Init(I2C0, &i2cInit);

	I2C0->CTRL = I2C_CTRL_AUTOSN;

	I2C_Enable(I2C0, true);
}

I2C_TransferReturn_TypeDef
i2c_master_read(uint16_t slaveAddress, uint8_t targetAddress,
		uint8_t * rxBuff, uint8_t numBytes)
{
	// Transfer structure
	I2C_TransferSeq_TypeDef i2cTransfer;
	I2C_TransferReturn_TypeDef result;

	// Initializing I2C transfer
	i2cTransfer.addr = slaveAddress;
	i2cTransfer.flags = I2C_FLAG_WRITE_READ;
	// i2cTransfer.flags = I2C_FLAG_READ;

	i2cTransfer.buf[0].data = &targetAddress;
	i2cTransfer.buf[0].len = 1;
	i2cTransfer.buf[1].data = rxBuff;
	i2cTransfer.buf[1].len = numBytes;

	result = I2C_TransferInit(I2C0, &i2cTransfer);

	// Sending data - GETS STUCK IN THIS LOOP
	while (result == i2cTransferInProgress)
	{
		result = I2C_Transfer(I2C0);
	}

	return result;
}
