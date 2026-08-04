
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "tcs3400.h"

LOG_MODULE_REGISTER(tcs3400);

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
static int tcs3400_whoami(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	uint8_t temp;
	int ret = 0;
	write_reg(dev, TCS3400_REGISTER_ENABLE, 0x03);
	ret = read_reg(dev, TCS3400_REGISTER_ENABLE, &temp, sizeof(temp));
	if (ret != 0) {
		return ret;
	}
	ret = read_reg(dev, TCS3400_REGISTER_ID, &temp, sizeof(temp));
	if (ret != 0) {
		return ret;
	}

	printk("WHO AM I Register reads  0x%02X  \n", temp);
	return 0;
}

static int tcs3400_rgb_reading(const struct device *dev, tcs3400_color_t *colors)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(colors);

	tcs3400_data_t *data = (tcs3400_data_t *)dev->data;

	uint8_t tmp[6];
	read_reg(dev, TCS3400_REGISTER_RDATAL, tmp, sizeof(tmp));

	colors->red = sys_get_be16(&tmp[0]);
	colors->blue = sys_get_be16(&tmp[2]);
	colors->green = sys_get_be16(&tmp[4]);

	return 0;
}

/**
 * @brief Initialize the tcs3400 driver.
 *
 * @param[in] dev tcs3400 device instance. Must not be NULL.
 *
 * @retval 0 Driver was initialized successfully.
 * @retval -EINVAL If @p dev is NULL, the I2C bus is NULL, or the configured
 * I2C address is invalid.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from register access or range configuration on failure.
 */
static int tcs3400_init(const struct device *dev)
{
	CHECK_NULL_PTR(dev);
	int ret = tcs3400_whoami(dev);
	if (ret != 0) {
		return -errno;
	}
}

int tcs3400_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(chan);

	tcs3400_data_t *data = (tcs3400_data_t *)dev->data;
	tcs3400_color_t colors;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_PRESS &&
	    chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}

	if (chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_PRESS) {
		ret = tcs3400_rgb_reading(dev, &colors); // Fixed: added '&'
		if (ret < 0) {
			return ret;
		}
		data->colors = colors;
	}

	return 0;
}

static int tcs3400_channel_get(const struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct tcs3400_data_t *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_LIGHT:
		val->val1 = sys_le16_to_cpu(data->sample_crgb[0]);
		val->val2 = 0;
		break;
	case SENSOR_CHAN_RED:
		val->val1 = sys_le16_to_cpu(data->sample_crgb[1]);
		val->val2 = 0;
		break;
	case SENSOR_CHAN_GREEN:
		val->val1 = sys_le16_to_cpu(data->sample_crgb[2]);
		val->val2 = 0;
		break;
	case SENSOR_CHAN_BLUE:
		val->val1 = sys_le16_to_cpu(data->sample_crgb[3]);
		val->val2 = 0;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static DEVICE_API(sensor, tcs3400_api) = {
	.sample_fetch = tcs3400_sample_fetch,
	.channel_get = tcs3400_channel_get,
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