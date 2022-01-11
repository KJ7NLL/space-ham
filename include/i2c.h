#include "em_i2c.h"

#define I2C_PORT gpioPortD
#define I2C_SDA 2
#define I2C_SCL 3

void initI2C();

I2C_TransferReturn_TypeDef i2c_master_read(uint16_t slaveAddress, uint8_t targetAddress,
	uint8_t * rxBuff, uint8_t numBytes);

