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

int main(void)
{
	int ret = 0;

	while (1) {
		ret = sensor_sample_fetch_chan(imu0, SENSOR_CHAN_ACCEL_XYZ);
		if (ret != 0) {
			LOG_ERR("Get Accel failed: %d", ret);
		}

		// Read and print accelerometer data
		struct sensor_value accel[3];
		ret = sensor_channel_get(imu0, SENSOR_CHAN_ACCEL_XYZ, &accel);

		LOG_PRINTK(">ax:%.3f,ay:%.3f,az:%.3f\n", sensor_value_to_double(&accel[0]),
			   sensor_value_to_double(&accel[1]), sensor_value_to_double(&accel[2]));

		k_msleep(500);
	}
}
