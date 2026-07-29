/**
 * @file mpu6050_bus.c
 * @brief I2C register access helpers for the MPU6050.
 *
 * @details Thin wrappers around the Zephyr I2C API for reading registers,
 * writing registers, and performing read-modify-write updates against a
 * masked field within a register. Used by every other source file in this
 * driver to talk to the device; contains no MPU6050-specific interpretation
 * of register contents.
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "mpu6050.h"

LOG_MODULE_DECLARE(mpu6050);

int mpu6050_read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;
	return i2c_write_read_dt(&cfg->i2c, &reg, 1, val, len);
}

int mpu6050_write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;
	return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

int mpu6050_write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t tmp;

	int ret = mpu6050_read_reg(dev, reg, &tmp, sizeof(tmp));
	if (ret != 0) {
		LOG_ERR("Error: %d", ret);
		return ret;
	}

	tmp &= ~mask;                 // Clear the target bit
	tmp |= FIELD_PREP(mask, val); // Set the target bit based on val

	return mpu6050_write_reg(dev, reg, tmp);
}
