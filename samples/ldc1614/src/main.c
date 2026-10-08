#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

const struct device *ldc1614_dev = DEVICE_DT_GET(DT_NODELABEL(ldc1614));

int main
{
	if (!device_is_ready(ldc1614_dev)) {
		LOG_ERR("LDC1614 device not ready");
		return 0;
	}
	while (1) {
		struct sensor_value val;
		int ret = sensor_sample_fetch(ldc1614_dev);
		if (ret != 0) {
			LOG_ERR("Failed to fetch sample: %d", ret);
			k_sleep(K_MSEC(1000));
			continue;
		}
		ret = sensor_channel_get(ldc1614_dev, SENSOR_CHAN_PROX, &val);
		if (ret != 0) {
			LOG_ERR("Failed to get channel data: %d", ret);
			k_sleep(K_MSEC(1000));
			continue;
		}
		LOG_INF("LDC1614 Proximity Data: %d", val.val1);
		k_sleep(K_MSEC(1000));
	}
}
