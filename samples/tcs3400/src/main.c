
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);

#define ERROR_LOOP(...)                                                                            \
	do {                                                                                       \
		while (1) {                                                                        \
			LOG_ERR(__VA_ARGS__);                                                      \
			k_msleep(1000);                                                            \
		}                                                                                  \
	} while (0)

int main(void)
{
	const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(p1));

	if (!device_is_ready(p1)) {
		LOG_ERR("Sensor device not ready!");
		return -ENODEV;
	}

	LOG_INF("device is ready \n");

	while (1) {

		k_msleep(1000);
	}
}
