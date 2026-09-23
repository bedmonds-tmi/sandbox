#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Get the node identifier from the overlay label */
#define TMAG5273_NODE DT_NODELABEL(tmag5273)

int main(void)
{
	const struct device *const dev = DEVICE_DT_GET(TMAG5273_NODE);
	struct sensor_value x, y, z;
	int ret;

	if (!device_is_ready(dev)) {
		LOG_ERR("Device %s is not ready", dev->name);
		return -ENODEV;
	}

	LOG_INF("Device %s ready. Starting 3-axis magnetic read loop...", dev->name);

	while (1) {
		/* Trigger a fetch from the sensor */
		ret = sensor_sample_fetch(dev);
		if (ret < 0) {
			LOG_ERR("Sample fetch failed: %d", ret);
			k_msleep(1000);
			continue;
		}

		/* Read the individual magnetic channels (values returned in Gauss) */
		ret = sensor_channel_get(dev, SENSOR_CHAN_MAGN_X, &x);
		if (ret < 0) {
			LOG_WRN("Failed to read X-axis: %d", ret);
		}

		ret = sensor_channel_get(dev, SENSOR_CHAN_MAGN_Y, &y);
		if (ret < 0) {
			LOG_WRN("Failed to read Y-axis: %d", ret);
		}

		ret = sensor_channel_get(dev, SENSOR_CHAN_MAGN_Z, &z);
		if (ret < 0) {
			LOG_WRN("Failed to read Z-axis: %d", ret);
		}

		/* Format and output to the serial console */
		printk("MAG [Gauss] -> X: %d.%06d | Y: %d.%06d | Z: %d.%06d\n", x.val1,
		       x.val2 < 0 ? -x.val2 : x.val2, y.val1, y.val2 < 0 ? -y.val2 : y.val2, z.val1,
		       z.val2 < 0 ? -z.val2 : z.val2);

		/* Alternatively with floating point enabled via CONFIG_CBPRINTF_FP_SUPPORT:
		 * printk("MAG [Gauss] -> X: %.4f | Y: %.4f | Z: %.4f\n",
		 *        sensor_value_to_double(&x),
		 *        sensor_value_to_double(&y),
		 *        sensor_value_to_double(&z));
		 */

		k_msleep(250);
	}

	return 0;
}
