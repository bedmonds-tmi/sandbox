#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(apps);

#define MAX_SAMPLES        200
#define SAMPLE_INTERVAL_US 32500

const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(p1));

int main(void)
{
	uint32_t start_time = k_uptime_get_32();
	uint32_t elapsed = 0;
	int sample_count = 0;
	int ret = 0;

	// Check if the device is actually ready before using it
	if (!device_is_ready(p1)) {
		LOG_ERR("Sensor device not ready!");
		return -ENODEV;
	}

	LOG_INF("DATA_START\n");
	LOG_INF("Time(ms),Pressure(Pa)\n");

	while (sample_count < MAX_SAMPLES) {
		struct sensor_value diff_pressure;
		struct sensor_value temp;

		double ans = 0;
		double anst = 0;
		ret = sensor_sample_fetch(p1);
		if (ret == 0) {

			ret = sensor_channel_get(p1, SENSOR_CHAN_PRESS, &diff_pressure);
			ret = sensor_channel_get(p1, SENSOR_CHAN_AMBIENT_TEMP, &temp);

			if (ret == 0) {
				elapsed = k_uptime_get_32() - start_time;

				// Convert the sensor_value structure safely to a double
				ans = sensor_value_to_double(&diff_pressure);
				anst = sensor_value_to_double(&temp);

				// Log the timestamp and the double value to 3 decimal places
				printk("press ,%0.3f\n", ans);
				printk("temp ,%0.3f\n", anst);

				sample_count++;
			}
		} else {
			LOG_WRN("Failed to fetch sample: %d", ret);
		}

		// Microsecond sleep for highly accurate 22.5ms intervals
		k_usleep(SAMPLE_INTERVAL_US);
	}

	LOG_INF("DATA_END\n");
	while (1) {
		k_msleep(1000);
	}
}
