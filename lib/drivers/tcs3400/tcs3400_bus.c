//  * @file tcs3400_bus.c
//  * @brief I2C register access helpers for the tcs3400.
//  *
//  * @details Thin wrappers around the Zephyr I2C API for reading registers,
//  * writing registers, and performing read-modify-write updates against a
//  * masked field within a register. Used by every other source file in this
//  * driver to talk to the device;
//  contains no specific interpretation *of register contents.* /

#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "tcs3400.h"

LOG_MODULE_DECLARE(tcs3400);

/**
 * @brief Read data from one or more registers.
 *
 * @param[in]  dev Pointer to the device structure.
 * @param[in]  reg The starting register address to read from.
 * @param[out] val Pointer to the buffer where data will be stored.
 * @param[in]  len Number of bytes to read from the device.
 *
 * @return 0 on success.
 * @return Negative error code on failure.
 */
int tcs3400_read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	const tcs3400_config_t *cfg = (const tcs3400_config_t *)dev->config;

	return i2c_write_read_dt(&cfg->i2c, &reg, 1, val, len);
}

/**
 * @brief Write a single byte to a register.
 *
 * @param[in] dev Pointer to the device structure.
 * @param[in] reg The register address to write to.
 * @param[in] val The byte value to write to the register.
 *
 * @return 0 on success.
 * @return Negative error code on failure.
 */
int tcs3400_write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	const tcs3400_config_t *cfg = (const tcs3400_config_t *)dev->config;

	return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

/**
 * @brief Update specific bits in a register using a mask.
 *
 * @details
 *  This function performs a read-modify-write operation. It reads
 * the current value, clears the masked bits, inserts the new value
 * shifted to fit the mask, and writes it back.
 *
 * @param[in] dev  Pointer to the device structure.
 * @param[in] reg  The register address to update.
 * @param[in] mask The bitmask defining which bits to change.
 * @param[in] val  The new value to place into the masked field.
 *
 * @return 0 on success.
 * @return Negative error code on failure.
 */
int tcs3400_write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t tmp;

	int ret = tcs3400_read_reg(dev, reg, &tmp, sizeof(tmp));
	if (ret != 0) {
		LOG_ERR("Error: %d", ret);
		return ret;
	}

	tmp &= ~mask;                 // Clear the target bit
	tmp |= FIELD_PREP(mask, val); // Set the target bit based on val

	return tcs3400_write_reg(dev, reg, tmp);
}
