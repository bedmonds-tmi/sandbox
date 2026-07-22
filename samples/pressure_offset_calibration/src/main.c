#include <zephyr/kernel.h>
#include <tmi/api/pressure.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(apps);

#define MAX_SAMPLES         200
#define CALIBRATION_SAMPLES 50
#define SAMPLE_INTERVAL_US  22500

const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(p1));

int main(void)
{
	int ret = tmi_pressure_init(p1);
	if (ret != 0) {
		LOG_ERR("Init failed: %d\n", ret);
		return ret;
	}

	int sample_count = 0;
	p1->data.offset = 0;
	double sum = 0;
	int32_t differential_pressure = 0;

	LOG_INF("Calibrating sensor...\n");

	/* 1. Calibration Phase: Take samples to find the average baseline offset */
	while (sample_count < CALIBRATION_SAMPLES) {
		ret = tmi_pressure_get_pressure_diff_mPa(p1, &differential_pressure);
		if (ret == 0) {
			sum += ((double)differential_pressure / 1000.0);
			sample_count++;
		}
		k_usleep(SAMPLE_INTERVAL_US);
	}

	/* 2. Update the offset variable with the optimal calculated average */
	p1->data.offset = sum / CALIBRATION_SAMPLES;
	LOG_INF("Calibration done. Optimal offset: %0.3f Pa\n", offset);

	/* Reset variables for the main data loop */
	sample_count = 0;
	uint32_t start_time = k_uptime_get_32();
	uint32_t elapsed = 0;

	LOG_INF("DATA_START\n");
	LOG_INF("Time(ms),Pressure(Pa)\n");

	/* 3. Data Collection Phase using the new offset */
	while (sample_count < MAX_SAMPLES) {
		double ans = 0;
		ret = tmi_pressure_get_pressure_diff_mPa(p1, &differential_pressure);
		if (ret == 0) {
			elapsed = k_uptime_get_32() - start_time;
			ans = ((double)differential_pressure / 1000.0) - p1->data.offset;
			LOG_INF("%u,%0.3f\n", elapsed, ans);
			sample_count++;
		}
		k_usleep(SAMPLE_INTERVAL_US);
	}

	LOG_INF("DATA_END\n");

	while (1) {
		k_msleep(1000);
	}
}
