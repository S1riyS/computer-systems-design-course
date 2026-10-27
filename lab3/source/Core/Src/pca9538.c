#include "pca9538.h"

#include "i2c.h"

bool pca9538_write(uint8_t addr, uint8_t reg, uint8_t value)
{
	return i2c_mem_write(addr, reg, &value, 1);
}

bool pca9538_read(uint8_t addr, uint8_t reg, uint8_t *value)
{
	return i2c_mem_read(addr, reg, value, 1);
}
