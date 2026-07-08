#include <zephyr/kernel.h>
#include <tmi/api/pressure.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(apps);

const struct device *p0 = DEVICE_DT_GET(DT_NODELABEL(p0));

int main(void)
{
	int ret = tmi_pressure_init(p0);
	if (ret != 0) {
		LOG_ERR("Init failed: %d\n", ret);
	}
	while (1) {
		k_msleep(1000);

		float humidity = 0;
		float pressure = 0;
		float temp = 0;
		ret = tmi_pressure_get_temp_mC(p0, &temp);
		if (ret != 0) {
			LOG_ERR("Init failed: %d\n", ret);
		}
		ret = tmi_pressure_get_humidity(p0, &humidity);
		if (ret != 0) {
			LOG_ERR("Init failed: %d\n", ret);
		}
		ret = tmi_pressure_get_pressure(p0, &pressure);
		if (ret != 0) {
			LOG_ERR("Init failed: %d\n", ret);
		}
		printk(">t:%0.3f \n", temp);
		printk(">h:%0.3f \n", humidity);
		printk(">p:%0.3f \n", pressure);
	}
}
