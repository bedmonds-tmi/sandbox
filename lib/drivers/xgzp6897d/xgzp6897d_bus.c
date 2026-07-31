#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "xgzp6897d.h"

/**
 * @brief reads a set value from a register of choice
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] reg  register you want to change
 * @param[out] val  a variable that will contain the contents read
 * @param[in] len  length of the register being read
 *
 * @retval 0        On success.
 * @return  -errno   Standard negative error code on failure.
 */
static int read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	const xgzp6897d_config_t *cfg = (const xgzp6897d_config_t *)dev->config;

	return i2c_write_read_dt(&cfg->i2c, &reg, 1, val, len);
}

/**
 * @brief writes a set value to a register of choice
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] reg  register you want to change
 * @param[in] val value you would like to replace the range of bits with
 *
 * @retval 0        On success.
 * @return -errno   Standard negative error code on failure.
 */
static int write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	const xgzp6897d_config_t *cfg = (const xgzp6897d_config_t *)dev->config;

	return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

/**
 * @brief writes a set value to a set # of bits
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] reg  register you want to change
 * @param[in] mask bit mask of 1's ranging section of desired change
 * @param[in] val value you would like to replace the range of bits with
 *
 * @retval 0        On success.
 * @return -errno   Standard negative error code on failure.
 */
static int write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t temp;
	int ret = read_reg(dev, reg, &temp, sizeof(temp));

	if (ret != 0) {
		LOG_ERR("Error: %d", ret);
		return ret;
	}

	temp &= ~mask;                 // Clear the target bit
	temp |= FIELD_PREP(mask, val); // Set the target bit based on val

	return write_reg(dev, reg, temp);
}
