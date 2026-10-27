#include "i2c.h"

#define I2C_MEM_TIMEOUT_MS 50u

static bool i2c_run(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len,
		    uint32_t timeout_ms, bool is_mem, bool is_read)
{
	HAL_StatusTypeDef status;

	if (is_mem && is_read) {
		status = HAL_I2C_Mem_Read(&hi2c1, addr, reg, I2C_MEMADD_SIZE_8BIT,
					  data, len, timeout_ms);
	} else if (is_mem) {
		status = HAL_I2C_Mem_Write(&hi2c1, addr, reg, I2C_MEMADD_SIZE_8BIT,
					   data, len, timeout_ms);
	} else {
		status = HAL_I2C_Master_Transmit(&hi2c1, addr, data, len, timeout_ms);
	}
	return status == HAL_OK;
}

/* Если обмен оборвался, ведомый мог оставить SDA в нуле.
 * Девять тактов SCL и STOP возвращают шину, затем контроллер включается заново. */
static void i2c_bus_recover(void)
{
	GPIO_InitTypeDef gpio = {0};

	HAL_I2C_DeInit(&hi2c1);

	__HAL_RCC_GPIOB_CLK_ENABLE();
	gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	gpio.Mode = GPIO_MODE_OUTPUT_OD;
	gpio.Pull = GPIO_PULLUP;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &gpio);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_SET);

	for (int pulse = 0; pulse < 9; pulse++) {
		if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_SET)
			break;
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
		for (volatile int delay = 0; delay < 2000; delay++) {
		}
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
		for (volatile int delay = 0; delay < 2000; delay++) {
		}
	}

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
	for (volatile int delay = 0; delay < 2000; delay++) {
	}
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
	for (volatile int delay = 0; delay < 2000; delay++) {
	}
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);

	MX_I2C1_Init();
}

static bool i2c_run_retry(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len,
			  uint32_t timeout_ms, bool is_mem, bool is_read)
{
	if (i2c_run(addr, reg, data, len, timeout_ms, is_mem, is_read))
		return true;
	i2c_bus_recover();
	return i2c_run(addr, reg, data, len, timeout_ms, is_mem, is_read);
}

bool i2c_mem_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
	return i2c_run_retry(addr, reg, (uint8_t *)data, len, I2C_MEM_TIMEOUT_MS, true, false);
}

bool i2c_mem_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
	return i2c_run_retry(addr, reg, data, len, I2C_MEM_TIMEOUT_MS, true, true);
}

bool i2c_master_write(uint8_t addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
	return i2c_run_retry(addr, 0, (uint8_t *)data, len, timeout_ms, false, false);
}

I2C_HandleTypeDef hi2c1;

void MX_I2C1_Init(void)
{
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 400000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
		Error_Handler();
	/* Как в пособии: аналоговый фильтр включён, цифровой выключен. */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
		Error_Handler();
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
		Error_Handler();
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *i2c)
{
	GPIO_InitTypeDef gpio = {0};

	if (i2c->Instance != I2C1)
		return;

	__HAL_RCC_GPIOB_CLK_ENABLE();
	gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	gpio.Mode = GPIO_MODE_AF_OD;
	gpio.Pull = GPIO_PULLUP;
	gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	gpio.Alternate = GPIO_AF4_I2C1;
	HAL_GPIO_Init(GPIOB, &gpio);

	__HAL_RCC_I2C1_CLK_ENABLE();
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *i2c)
{
	if (i2c->Instance != I2C1)
		return;

	__HAL_RCC_I2C1_CLK_DISABLE();
	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8 | GPIO_PIN_9);
}
