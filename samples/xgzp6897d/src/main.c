#include <zephyr/kernel.h>
#include <tmi/api/pressure.h>
#include <zephyr/logging/log.h>
<<<<<<< HEAD
#include <stdio.h>
	== == ==
	=
#include <zephyr/fs/fs.h>
#include <stdio.h>
#include "hardware_config.c"
		>>>>>>> 745bcf5 (drivers: xgzp6897d: add dualport pressure sensor) LOG_MODULE_REGISTER(apps);

#define MAX_SAMPLES        200
#define SAMPLE_INTERVAL_US 22500

const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(p1));

int main(void)
{
	int ret = tmi_pressure_init(p1);
	tmi_pressure_whoami(p1);
	if (ret != 0) {
		LOG_ERR("Init failed: %d\n", ret);
		return ret;
	}
	uint32_t start_time = k_uptime_get_32();
	uint32_t elapsed = 0;
	int sample_count = 0;
	double offset = -14.566975;

	LOG_INF("DATA_START\n");
	LOG_INF("Time(ms),Pressure(Pa),Pressure(mmH20)\n");

	while (sample_count < MAX_SAMPLES) {
		float pressure = 0;
		double ans = 0;
		ret = tmi_pressure_get_pressure(p1, &pressure);
		if (ret == 0) {
			elapsed = k_uptime_get_32() - start_time;
			ans = pressure - offset;
			LOG_INF("%u,%0.3f,%0.3f\n", elapsed, ans, (ans) * 0.101972);
			sample_count++;
		}
		/* Use microsecond sleep to accurately hit the 22.5ms intervals */
		k_usleep(SAMPLE_INTERVAL_US);
	}

	LOG_INF("DATA_END\n");

	while (1) {
		k_msleep(1000);
	}
}
