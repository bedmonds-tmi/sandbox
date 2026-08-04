
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "tcs3400.h"

#define TCS3400_I2C_ADDRESS 0x39
#define TCS3400_ID_1        0x90
#define TCS3400_ID_2        0x93

LOG_MODULE_REGISTER(tcs3400, CONFIG_TMI_DRIVER_TCS3400_LOG_LEVEL);

/**
 * @brief Reads CRGB.
 *
 * @details
 * Reads the RGB values from the TCS3400 and stores them in the device data.
 * Uses the sys_get_le16() helper to convert the little-endian register values to host-endian
 * uint16_t values.
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[out] val Destination for the rgb values. Must not be NULL.
 *
 * @retval 0 successfully read the rgb values.
 * @retval -EINVAL If parameters are NULL.
 */
static int tcs3400_rgb_reading(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	tcs3400_data_t *data = (const tcs3400_data_t *)dev->data;

	uint8_t tmp[8];
	tcs3400_read_reg(dev, TCS3400_REG_CDATAL, tmp, sizeof(tmp));

	uint16_t clear = sys_get_le16(&tmp[0]);
	uint16_t red = sys_get_le16(&tmp[2]);
	uint16_t blue = sys_get_le16(&tmp[4]);
	uint16_t green = sys_get_le16(&tmp[6]);

	LOG_DBG("clear = %02X, red = %02X, blue = %02X, green = %02X \n ", tmp[0], tmp[2], tmp[4],
		tmp[6]);

	data->colors[0].val1 = clear;
	data->colors[1].val1 = red;
	data->colors[2].val1 = blue;
	data->colors[3].val1 = green;

	return 0;
}

/**
 * @brief Default setup write function to the device.
 *
 * @details
 * Writes default values to the TCS3400 registers to initialize the device. This includes setting
 * the integration time, gain, and enabling the device.
 * @param[in] dev  device instance. Must not be NULL.
 *
 * @retval 0 Temperature was read successfully.
 * @retval -EINVAL If parameters are NULL.
 */
static int tcs3400_sensor_setup(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	const tcs3400_config_t *cfg = (const tcs3400_config_t *)dev->config;

	uint8_t chip_id;
	int ret;
	struct {
		uint8_t reg_addr;
		uint8_t value;
	} reset_regs[] = {
		{TCS3400_REG_ENABLE, TCS3400_DEFAULT_ENABLE},
		{TCS3400_REG_AICLEAR, TCS3400_AICLEAR_RESET},
		{TCS3400_REG_RGBC_INTEGRATION, TCS3400_DEFAULT_ATIME},
		{TCS3400_REG_INTERRUPT_PERSISTENCE_FILTER, TCS3400_DEFAULT_PERS},
		{TCS3400_REG_CONFIG, TCS3400_DEFAULT_CONFIG},
		{TCS3400_REG_CTRL, TCS3400_DEFAULT_CTRL},
	};

	ret = i2c_reg_read_byte_dt(&cfg->i2c, TCS3400_REG_ID, &chip_id);
	if (ret) {
		LOG_DBG("Failed to read chip id: %d", ret);
		return ret;
	}

	if (!((chip_id == TCS3400_ID_1) || (chip_id == TCS3400_ID_2))) {
		LOG_DBG("Invalid chip id: %02x", chip_id);
		return -EIO;
	}

	LOG_INF("chip id: 0x%x", chip_id);

	for (size_t i = 0; i < ARRAY_SIZE(reset_regs); i++) {
		ret = i2c_reg_write_byte_dt(&cfg->i2c, reset_regs[i].reg_addr, reset_regs[i].value);
		if (ret) {
			LOG_ERR("Failed to set default register: %02x", reset_regs[i].reg_addr);
			return ret;
		}
	}
	uint8_t temp;
	ret = tcs3400_write_reg(dev, TCS3400_REG_ENABLE, 0x03);
	if (ret != 0) {
		return ret;
	}
	ret = tcs3400_read_reg(dev, TCS3400_REG_ENABLE, &temp, sizeof(temp));
	if (ret != 0) {
		return ret;
	}

	LOG_DBG("ENABLE Register: 0x%02X", temp);
	// DEBUG TEST print
	ret = tcs3400_write_reg(dev, TCS3400_REG_CTRL, 0x00);
	if (ret != 0) {
		return ret;
	}
	ret = tcs3400_write_reg(dev, TCS3400_REG_INTERRUPT_PERSISTENCE_FILTER, 0);
	if (ret == 0) {
		return ret;
	}

	ret = tcs3400_write_reg(dev, TCS3400_REG_WAIT_TIME, 0xFF);
	if (ret != 0) {
		return ret;
	}

	ret = tcs3400_write_reg(dev, TCS3400_REG_CTRL, 3);
	if (ret != 0) {
		return ret;
	}

	ret = tcs3400_write_reg(dev, TCS3400_REG_RGBC_INTEGRATION, 0xFF);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

/**
 * @brief Read the temperature.
 *
 * @details
 * Sets the RGB gain factor for the TCS3400. The gain factor is used to scale the raw RGB values
 * read from the device.
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] scale Desired gain scale. Must not be NULL.
 *
 * @retval 0 Temperature was read successfully.
 * @retval -EINVAL If parameters are NULL.
 */
static int tcs3400_set_rgb_gain_factor(const struct device *dev, uint32_t scale)
{
	CHECK_NULL_PTR(dev);

	tcs3400_data_t *data = (tcs3400_data_t *)dev->data;
	tcs3400_color_gain_t gain_fs = tcs3400_scale_gain(scale);

	int ret = tcs3400_write_mask(dev, TCS3400_REG_CTRL, TCS3400_MASK_CTRL_AGAIN,
				     (uint8_t)gain_fs);

	if (ret != 0) {
		return ret;
	}

	data->gain = gain_fs;

	return 0;
}

/**
 * @brief Read the sample.
 *
 * @details
 * collects a sample from the TCS3400 device and stores it in the device data.
 * This function reads the RGB values from the device and updates the corresponding
 * fields in the device data structure.
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] chan  channel Must not be NULL.
 *
 * @retval 0 sample was read successfully.
 * @retval -EINVAL If parameters are NULL.
 */
static int tcs3400_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(chan);

	if (chan == SENSOR_CHAN_ALL) {
		int ret = tcs3400_rgb_reading(dev); // Fixed: added '&'
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

/**
 * @brief collects a sample from the TCS3400 device and stores it in the device data.
 *
 * @details
 * This function reads the RGB values from the device and updates the corresponding
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] chan  channel Must not be NULL.
 * @param[out] val Destination for the
 * rgb values. Must not be NULL.
 *
 * @retval 0 Temperature was read successfully.
 * @retval -EINVAL If parameters are NULL.
 */
static int tcs3400_channel_get(const struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(val);

	tcs3400_data_t *data = (tcs3400_data_t *)dev->data;
	switch (chan) {
	case SENSOR_CHAN_LIGHT:
		LOG_DBG("clear = %d, red = %d, blue = %d, green = %d  \n", data->colors[0].val1,
			data->colors[1].val1, data->colors[2].val1, data->colors[3].val1);

		val[0] = data->colors[0];
		val[1] = data->colors[1];
		val[2] = data->colors[2];
		val[3] = data->colors[3];

		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief Initialize the TCS3400 driver.
 *
 * @param[in] dev TCS3400 device instance. Must not be NULL.
 *
 * @retval 0 Driver was initialized successfully.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -EFAULT If the I2C bus is not ready.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from register access or range configuration on
 * failure.
 */
static int tcs3400_init(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	const tcs3400_config_t *cfg = (const tcs3400_config_t *)dev->config;

	bool ready = i2c_is_ready_dt(&cfg->i2c);
	if (!ready) {
		LOG_ERR("I2C bus is not ready.");
		return -EFAULT;
	}

	int ret = tcs3400_sensor_setup(dev);
	if (ret != 0) {
		return -errno;
	}

	LOG_DBG("Initialized.");

	return 0;
}

static DEVICE_API(sensor, tcs3400_api) = {
	.sample_fetch = tcs3400_sample_fetch,
	.channel_get = tcs3400_channel_get,
#ifdef CONFIG_TMI_DRIVER_TCS3400_TRIGGER
	.trigger_set = tcs3400_trigger_set,
#endif
};

#define DT_DRV_COMPAT tmi_tcs3400

#define XGZP6897_DEFINE(inst)                                                                      \
	static tcs3400_data_t tcs3400_data_##inst;                                                 \
                                                                                                   \
	static const tcs3400_config_t tcs3400_config_##inst = {                                    \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, tcs3400_init, NULL, &tcs3400_data_##inst,                      \
			      &tcs3400_config_##inst, POST_KERNEL,                                 \
			      TMI_DRIVER_tcs3400_INIT_PRIORITY, &tcs3400_api);

DT_INST_FOREACH_STATUS_OKAY(XGZP6897_DEFINE)
