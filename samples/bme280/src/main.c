#include <zephyr/kernel.h>
#include <tmi/api/pressure.h>
#include <zephyr/logging/log.h>
#include "hardware_config.c"

LOG_MODULE_REGISTER(apps);

extern const tmi_pressure_api_t xgzp6897d_api;

int main(void)
{
	int ret = tmi_pressure_init(&p0);
	tmi_pressure_whoami(&p0);
	if (ret != 0) {
		LOG_ERR("Init failed: %d\n", ret);
	}
	while (1) {
		k_msleep(1000);

		float humidity, pressure;
		int temp = 0;
		ret = tmi_pressure_get_temp_mC(&p0, &temp);
		if (ret != 0) {
			LOG_ERR("Init failed: %d\n", ret);
		}
		ret = tmi_pressure_get_humidity(&p0, &humidity);
		if (ret != 0) {
			LOG_ERR("Init failed: %d\n", ret);
		}
		ret = tmi_pressure_get_pressure(&p0, &pressure);
		if (ret != 0) {
			LOG_ERR("Init failed: %d\n", ret);
		}
		LOG_PRINTK(">t:%0.3f \n", temp);
		LOG_PRINTK(">h:%0.3f \n", humidity);
		LOG_PRINTK(">p:%0.3f \n", pressure);
	}
}
