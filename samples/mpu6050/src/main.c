#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);

const struct device *imu0 = DEVICE_DT_GET(DT_NODELABEL(imu0));

#define ERROR_LOOP(...)                                                                            \
	do {                                                                                       \
		while (1) {                                                                        \
			LOG_ERR(__VA_ARGS__);                                                      \
			k_msleep(1000);                                                            \
		}                                                                                  \
	} while (0)

static void print_accel(const struct device *dev)
{
	struct sensor_value accel[3];
	int ret = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel);
	if (ret != 0) {
		LOG_ERR("Get Accel failed: %d", ret);
		return;
	}

	LOG_PRINTK(">ax:%.3f,ay:%.3f,az:%.3f\n", sensor_value_to_double(&accel[0]),
		   sensor_value_to_double(&accel[1]), sensor_value_to_double(&accel[2]));
}

/**
 * @brief Fires from the driver's interrupt thread/workqueue whenever a new
 * sample is ready, so no polling loop is needed on boards that have an
 * int-gpios pin wired up.
 */
static void data_ready_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	int ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);
	if (ret != 0) {
		LOG_ERR("Get Accel failed: %d", ret);
		return;
	}

	print_accel(dev);
}

int main(void)
{
	const struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ACCEL_XYZ,
	};

	/* -ENOTSUP means the board has no int-gpios wired up for imu0; fall
	 * back to polling so the same app works either way. */
	int ret = sensor_trigger_set(imu0, &trig, data_ready_handler);
	if (ret == 0) {
		LOG_INF("Data-ready interrupt configured, waiting for samples");
		while (1) {
			k_sleep(K_FOREVER);
		}
	} else if (ret != -ENOTSUP) {
		ERROR_LOOP("Failed to set data ready trigger: %d", ret);
	}

	LOG_INF("No int-gpios for imu0, polling instead");

	while (1) {
		ret = sensor_sample_fetch_chan(imu0, SENSOR_CHAN_ACCEL_XYZ);
		if (ret != 0) {
			LOG_ERR("Get Accel failed: %d", ret);
		}

		print_accel(imu0);

		k_msleep(500);
	}
}
