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
	double avg = 0;
	double sum = 0;
	double offset = 0;

	if (!device_is_ready(p1)) {
		LOG_ERR("Sensor device not ready!");
		return -ENODEV;
	}

	LOG_INF("DATA_START\n");

	// PHASE 1: CALIBRATION (Find Offset)
	LOG_INF("Starting calibration...");
	while (sample_count < MAX_SAMPLES) {
		struct sensor_value diff_pressure;
		double ans = 0;

		ret = sensor_sample_fetch(p1);
		if (ret == 0) {
			ret = sensor_channel_get(p1, SENSOR_CHAN_PRESS, &diff_pressure);
			if (ret == 0) {
				ans = sensor_value_to_double(&diff_pressure);
				sum += ans;
				sample_count++;
			}
		} else {
			LOG_WRN("Failed to fetch calibration sample: %d", ret);
		}
		k_usleep(SAMPLE_INTERVAL_US);
	}

	offset = sum / MAX_SAMPLES;
	LOG_INF("OFFSET calibration is done. Offset = %0.3f PA", offset);

	// RESET VARIABLES FOR MEASUREMENT
	sum = 0;
	sample_count = 0;

	// PHASE 2: MEASUREMENT (Apply Offset)
	LOG_INF("Starting measurement...");
	LOG_INF("Time(ms),Pressure(Pa)\n");

	while (sample_count < MAX_SAMPLES) {
		struct sensor_value diff_pressure;
		double ans = 0;

		ret = sensor_sample_fetch(p1);
		if (ret == 0) {
			ret = sensor_channel_get(p1, SENSOR_CHAN_PRESS, &diff_pressure);
			if (ret == 0) {
				elapsed = k_uptime_get_32() - start_time;
				ans = sensor_value_to_double(&diff_pressure);

				// Incorporate the calibrated offset
				ans = ans - offset;

				LOG_INF("%u,%0.3f\n", elapsed, ans);
				sum += ans;
				sample_count++;
			}
		} else {
			LOG_WRN("Failed to fetch measurement sample: %d", ret);
		}
		k_usleep(SAMPLE_INTERVAL_US);
	}

	avg = sum / MAX_SAMPLES;
	LOG_INF("DATA_END\nFINAL AVG = %0.3f\n", avg);

	// Infinite sleep loop
	while (1) {
		k_msleep(1000);
	}
}
