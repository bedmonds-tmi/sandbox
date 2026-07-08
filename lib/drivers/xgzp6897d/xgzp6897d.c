
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "xgzp6897d.h"

LOG_MODULE_REGISTER(xgzp6897d);

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

/**
 * @brief Verify communication with the DualPortDevice.
 *
 * @details Reads the WHOAMI register and checks that it matches the configured
 * I2C address.
 *
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

	uint8_t temp;

	int ret = read_reg(dev, XGZP6897D_REGISTER_WHOAMI, &temp, sizeof(temp));
	if (ret != 0) {
		return ret;
	}

	printk("WHO AM I Register reads  0x%02X  \n", temp);
	return 0;
}

/**
 * @brief updates the float variable passed to it with the temperature
 *
 * @details Reads 2 registers of tempurature data then uses preset values stored in a seperate
 * register for calculating the true tempurature value in degrees celcicus
 * follows this formula Final_T = (Inter_T- Byte1) / 2 ^ Shift_N + 25
 * Shift_N = Byte2/10
 * Byte1 is calculated from the 0x20
 * Inter_T = RAW_T- 65536  or when  (RAW_T > 32768); Inter_T = RAW_T
 *
 * @param[in] dev  device instance.
 * @param[out] temp_C uint32_t type.
 *
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_get_temperature_mC(const struct device *dev, uint32_t *temp_C)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(temp_C);

	uint8_t tmp[2] = {0, 0};
	uint8_t byte1_raw = 0;
	uint8_t byte2 = 0;

	read_reg(dev, XGZP6897D_REGISTER_TEMPURATURE_H, tmp, sizeof(tmp));
	uint16_t raw = ((uint16_t)tmp[0] << 8) | (uint16_t)tmp[1];

	read_reg(dev, XGZP6897D_REGISTER_CALCOE, &byte1_raw, sizeof(byte1_raw));
	read_reg(dev, XGZP6897D_REGISTER_CALCOE_2, &byte2, sizeof(byte2));

	uint8_t Shift_N = byte2 / 10;
	int32_t interT = 0;

	if (raw > 32768) {
		interT = (int32_t)raw - 65536;
	} else {
		interT = (int32_t)raw;
	}

	uint32_t power = 1U << Shift_N;
	uint32_t byte1 = 0;
	uint8_t pow = 0;
	bool sign = false;

	sign = byte1_raw & 0x80;
	pow = byte1_raw & 0x7F;
	byte1 = 1U << pow;

	int32_t Final_T = 0;

	if (sign) {
		int32_t Numerator = interT + (int32_t)byte1;
		Final_T = Numerator / (int32_t)power;
	} else {
		int32_t Numerator = interT - (int32_t)byte1;
		Final_T = Numerator / (int32_t)power;
	}

	Final_T = Final_T + 25;

	// Guard against negative temperatures before casting to unsigned
	if (Final_T < 0) {
		*temp_C = 0;
	} else {
		*temp_C = (uint32_t)Final_T;
	}

	return 0;
}

/**
 * @brief updates the int32_t variable passed to it with the pressure
 *
 * @param[in] dev  device instance.
 * @param[out] press_mPa int32_t type.
 *
 * @details Reads 3 registers of pressure data then uses preset PMIN and PMAX for
 * calibrating the bounds of the pressure difference read.
 *
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_get_diff_pressure_mPa(const struct device *dev, int32_t *press_mPa)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(press_mPa);
	const xgzp6897d_data_t *data = (const xgzp6897d_data_t *)dev->data;

	uint8_t tmp[3];
	read_reg(dev, XGZP6897D_REGISTER_PRESSURE_H, tmp, sizeof(tmp));

	// Combine 24-bit raw pressure data
	uint32_t raw = ((uint32_t)tmp[0] << 16) | ((uint32_t)tmp[1] << 8) | (uint32_t)tmp[2];
	int32_t pressure_mpa = 0;

	// 8388608 is 2^23 (the sign bit for 24-bit two's complement)
	if (raw & 0x800000) {
		// Handle negative pressure (2's complement conversion)
		// (raw - 2^24) * PRESSURE_SCALE * 1000 / 2^21
		int64_t intermediate = (int64_t)raw - 16777216; // 1 << 24
		pressure_mpa = (int32_t)((intermediate * PRESSURE_SCALE * 1000) >> 21);
	} else {
		// Handle positive pressure
		// raw * PRESSURE_SCALE * 1000 / 2^21
		int64_t intermediate = (int64_t)raw;
		pressure_mpa = (int32_t)((intermediate * PRESSURE_SCALE * 1000) >> 21);
	}

	*press_mPa = pressure_mpa;

	return 0;
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

int xgzp6897d_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	xgzp6897d_data_t *data = dev->data;
	int32_t press = 0;
	uint32_t temp = 0;
	int ret;
	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_PRESS &&
	    chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}
	if (chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_PRESS) {
		ret = xgzp6897d_get_diff_pressure_mPa(dev, &press); // Fixed: added '&'
		if (ret < 0) {
			return ret; // Propagate the error code to the caller
		}
		// printk("Temperature after function execution : %d \n", press);
		data->pressure = press;
	}

	if (chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_AMBIENT_TEMP) {
		// Replace this with your actual hardware temperature reading function
		ret = xgzp6897d_get_temperature_mC(dev, &temp);
		if (ret < 0) {
			return ret;
		}
		printk("Temperature after function execution : %d \n", temp);
		data->temperature = temp;
	}

	return 0;
}

int xgzp6897d_channel_get(const struct device *dev, enum sensor_channel chan,
			  struct sensor_value *val)
{
	CHECK_NULL_PTR(dev);
	const xgzp6897d_data_t *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_PRESS:
		// Zephyr expects kPa. 1 kPa = 1,000,000 mPa.
		// val1 = integer part, val2 = fractional part in millionths.
		val->val1 = data->pressure / 1000;
		val->val2 = data->pressure % 1000 * 1000;
		// printk("val1 : %d val2 : %d \n", val->val1, val->val2);
		return 0;

	case SENSOR_CHAN_AMBIENT_TEMP:
		// Zephyr expects °C. 1°C = 1,000 mC.
		// Convert millicelsius to micro-celsius (millionths) for val2
		val->val1 = data->temperature / 1;
		val->val2 = (data->temperature % 1) * 1U;
		printk("val1 : %d val2 : %d \n", val->val1, val->val2);
		return 0;

	default:
		return -ENOTSUP;
	}
}

static DEVICE_API(sensor, xgzp6897d_api) = {
	.sample_fetch = xgzp6897d_sample_fetch,
	.channel_get = xgzp6897d_channel_get,
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
