
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/byteorder.h>
#include <tmi/api/pressure.h>
#include <zephyr/logging/log.h>
#include "xgzp6897d.h"

LOG_MODULE_REGISTER(xgzp6897d);

#define XGZP6897D_REGISTER_WHOAMI 0x00

#define XGZP6897D_REGISTER_CONTROL       0x01
#define XGZP6897D_REGISTER_OSR           0x02
#define XGZP6897D_REGISTER_MEASURE       0x03
#define XGZP6897D_REGISTER_PRESSURE_H    0x04
#define XGZP6897D_REGISTER_TEMPURATURE_H 0x07
#define XGZP6897D_REGISTER_CALCOE        0x20
#define XGZP6897D_REGISTER_CALCOE_2      0x21

// XGZP6897DC005HPDPN range selected based on part # listed in data sheet
#define PMAX           500
#define PMIN           -500
#define PRESSURE_SCALE (PMAX - PMIN)

#define CHECK_NULL_PTR(ptr)                                                                        \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR("%s: null pointer: " #ptr, __func__);                              \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

/**
 * @brief reads a set value from a register of choice
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] reg  register you want to change
 * @param[in] val  a variable that will contain the contents read
 * @param[in] len  length of the register being read
 *
 * @retval 0        On success.
 * @retval -errno   Standard negative error code on failure.
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
 * @retval -errno   Standard negative error code on failure.
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
 * @retval -errno   Standard negative error code on failure.
 */
static int write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t tmp;

	int ret = read_reg(dev, reg, &tmp, sizeof(tmp));
	if (ret != 0) {
		LOG_ERR("Error: %d", ret);
		return ret;
	}
	tmp &= ~mask;                 // Clear the target bit
	tmp |= FIELD_PREP(mask, val); // Set the target bit based on val
	return write_reg(dev, reg, tmp);
}

/**
 * @brief Verify communication with the DualPortDevice.
 *
 * @details Reads the WHOAMI register and checks that it matches the configured
 * I2C address.
 * @param[in] dev  device instance. Must not be NULL.
 *
 * @retval 0 The expected WHOAMI value was read.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_whoami(const struct device *dev)
{
	CHECK_NULL_PTR(dev);
	uint8_t tmp;
	int ret = read_reg(dev, XGZP6897D_REGISTER_WHOAMI, &tmp, sizeof(tmp));
	if (ret != 0) {
		return ret;
	}
	printk("WHO AM I Register reads  0x%02X  \n", tmp);
	return 0;
}
/**
 * @brief updates the float variable passed to it with the temperature
 *
 * @param[in] dev  device instance.
 * @param[in] temp_C Float type.
 *
 * @details Reads 2 registers of tempurature data then uses preset values stored in a seperate
 * register for calculating the true tempurature value in degrees celcicus
 * follows this formula Final_T = (Inter_T- Byte1) / 2 ^ Shift_N + 25
 * Shift_N = Byte2/10
 * Byte1 is calculated from the 0x20
 *  Inter_T = RAW_T- 65536  or when  (RAW_T > 32768); Inter_T = RAW_T

 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_get_temp(const struct device *dev, double *temp_C)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(temp_C);
	uint8_t tmp[2] = {0, 0};
	uint8_t byte1_raw = 0;
	uint8_t byte2 = 0;
	read_reg(dev, XGZP6897D_REGISTER_TEMPURATURE_H, tmp, sizeof(tmp));
	uint16_t raw = (uint16_t)tmp[0] << 8 | (uint16_t)tmp[1];
	read_reg(dev, XGZP6897D_REGISTER_CALCOE, &byte1_raw, sizeof(byte1_raw));
	read_reg(dev, XGZP6897D_REGISTER_CALCOE_2, &byte2, sizeof(byte2));
	uint8_t Shift_N = byte2 / 10;
	int16_t interT = 0;
	if (raw > 32768) {
		interT = raw - 65536;
	} else {
		interT = raw;
	}
	uint32_t power = 1 << Shift_N;
	uint32_t byte1 = 0;
	uint8_t pow = 0;
	bool sign = false;
	sign = byte1_raw & 0x80;
	pow = byte1_raw & 0x7F;
	byte1 = 1 << pow;
	int16_t Final_T = 0;
	if (sign) {
		int16_t Numerator = interT + byte1;
		Final_T = (Numerator / (power));
	} else {
		int16_t Numerator = interT - byte1;
		Final_T = (Numerator / (power));
	}
	Final_T = Final_T + 25;
	*temp_C = Final_T;
	return 0;
}

/**
 * @brief updates the float variable passed to it with the pressure
 *
 * @param[in] dev  device instance.
 * @param[in] press_Pa Float type.
 *
 * @details Reads 3 registers of pressure data then uses preset PMIN and PMAX for
 * calibrating the bounds of the pressure difference read.
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_get_pressure(const struct device *dev, float *press_Pa)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(press_Pa);
	uint8_t tmp[3];
	read_reg(dev, XGZP6897D_REGISTER_PRESSURE_H, tmp, sizeof(tmp));
	uint32_t raw = (uint32_t)tmp[0] << 16 | (uint32_t)tmp[1] << 8 | (uint32_t)tmp[2];
	float power = (1 << 21);
	float powerv2 = (1 << 24);
	float pressure_pa = 0;
	if (raw >= 8388608) {
		pressure_pa = ((float)((float)raw - powerv2) / power) * PRESSURE_SCALE;
	} else {
		pressure_pa = ((float)(raw) / power) * PRESSURE_SCALE;
	}
	*press_Pa = pressure_pa;
	return 0;
}
/**
 * @brief is not initialized for this device
 *
 * @param[in] dev  device instance.
 * @param[in] RH
 * @details function parameters are required to have though this function does nothign
 * @retval 0 action prefromes nothing
 *  */
static int xgzp6897d_get_humidity(const struct device *dev, float *RH)
{
	LOG_PRINTK("Device cannot measure this parameter");
	return ENOTSUP;
}
/**
 * @brief Initialize the xgzp6897d driver.
 *
 * @param[in] dev xgzp6897d device instance. Must not be NULL.
 *
 * @retval 0 Driver was initialized successfully.
 * @retval -EINVAL If @p dev is NULL, the I2C bus is NULL, or the configured
 * I2C address is invalid.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from register access or range configuration on failure.
 */
static int xgzp6897d_init(const struct device *dev)
{
	CHECK_NULL_PTR(dev);
	const xgzp6897d_config_t *cfg = (const xgzp6897d_config_t *)dev->config;
	bool ready = i2c_is_ready_dt(&cfg->i2c);
	if (!ready) {
		LOG_ERR("I2C bus is not ready.");
		return -EFAULT;
	}
	int ret = xgzp6897d_whoami(dev);
	if (ret != 0) {
		return -ERANGE;
	}
	LOG_INF("Initialized");
	return 0;
}

const tmi_pressure_api_t xgzp6897d_api = {
	.init = xgzp6897d_init,
	.get_tempurature = xgzp6897d_get_temp,
	.get_humidity = xgzp6897d_get_humidity,
	.get_pressure = xgzp6897d_get_pressure,
	.get_whoami = xgzp6897d_whoami,
};

#define DT_DRV_COMPAT tmi_xgzp6897d

#define XGZP6897_DEFINE(inst)                                                                      \
	static xgzp6897d_data_t xgzp6897d_data_##inst;                                             \
                                                                                                   \
	static const xgzp6897d_config_t xgzp6897d_config_##inst = {                                \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, xgzp6897d_init, NULL, &xgzp6897d_data_##inst,                  \
			      &xgzp6897d_config_##inst, POST_KERNEL,                               \
			      TMI_DRIVER_XGZP6897D_INIT_PRIORITY, &xgzp6897d_api);
DT_INST_FOREACH_STATUS_OKAY(XGZP6897_DEFINE)
