#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <math.h>

#define M_PI                 3.14159265358979323846
#define LDC1614_FCLK_HZ      43400000.0
#define LDC1614_FIN_DIVIDER  1.0
#define LDC1614_FREF_DIVIDER 1.0
#define LDC1614_TANK_CAP_PF  330.0

void ldc1614_convert_raw(uint32_t raw_data, double *freq_hz_out, double *ind_uh_out)
{
	if (raw_data == 0) {
		*freq_hz_out = 0.0;
		*ind_uh_out = 0.0;
		return;
	}
	double f_ref = LDC1614_FCLK_HZ / LDC1614_FREF_DIVIDER;
	double f_sensor_hz = ((double)raw_data * f_ref * LDC1614_FIN_DIVIDER) / 268435456.0;
	*freq_hz_out = f_sensor_hz;
	double f_sensor_mhz = f_sensor_hz / 1000000.0;
	double omega_mhz = 2.0 * M_PI * f_sensor_mhz;
	*ind_uh_out = 1000000.0 / (omega_mhz * omega_mhz * LDC1614_TANK_CAP_PF);
}

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	const struct device *ldc1614_dev = DEVICE_DT_GET(DT_NODELABEL(dev0));

	if (ldc1614_dev == NULL) {
		LOG_ERR("Failed to get LDC1614 device");
		return 0;
	}

	if (!device_is_ready(ldc1614_dev)) {
		LOG_ERR("LDC1614 device not ready");
		return 0;
	}
	k_msleep(1);

	while (1) {
		struct sensor_value val;
		k_sleep(K_MSEC(2000));
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
		LOG_INF("LDC1614 Raw Data: %d", val.val1);

		double fr = 0.0;
		double inductance = 0.0;

		ldc1614_convert_raw(val.val1, &fr, &inductance);
		uint32_t fr_int = (uint32_t)fr / 1000;
		uint32_t fr_rem = (uint32_t)fr % 1000;

		uint32_t inductance_int = (uint32_t)inductance;
		uint32_t inductance_rem = (uint32_t)((inductance - inductance_int) * (1000));

		LOG_INF("LDC1614 inductance %u.%03u uH", inductance_int, inductance_rem);
		LOG_INF("LDC1614 frequency  %u.%08u Hz", fr_int, fr_rem);

		k_sleep(K_MSEC(10));
	}
};
