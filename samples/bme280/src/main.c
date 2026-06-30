#include <zephyr/kernel.h>
#include <tmi/api/pressure.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(apps);

extern const tmi_pressure_api_t bme280_api;
#define PRESSURE0_API bme280_api

tmi_pressure_t p0 = {
	.config =
		{
			.bus = DEVICE_DT_GET(DT_ALIAS(i2c0)),
			.addr = 0x77,
		},
	.api = &PRESSURE0_API,
};

int main(void)
{
	int ret = p0.api->init(&p0);
	tmi_pressure_whoami(&p0);
	if (ret != 0) {
		LOG_ERR("Init failed: %d\n", ret);
	}
	while (1) {
		k_msleep(1000);

		float temp, humidity, pressure;
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
