
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

#define TCS3400_ID_1 0x90
#define TCS3400_ID_2 0x93

#define FIXED_PERIOD   PWM_MSEC(1U)
#define FADE_STEP_SIZE (FIXED_PERIOD / 500U)
#define FADE_DELAY_MS  100

LOG_MODULE_REGISTER(app);

#define ERROR_LOOP(...)                                                                            \
	do {                                                                                       \
		while (1) {                                                                        \
			LOG_ERR(__VA_ARGS__);                                                      \
			k_msleep(1000);                                                            \
		}                                                                                  \
	} while (0)

static const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(p1));
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));

int main(void)
{
	int ret = 0;
	struct sensor_value colors[4];
	uint32_t pulse_width = 0U;
	uint8_t fading_up = 1U;

	if (!device_is_ready(p1)) {
		LOG_ERR("Sensor device not ready!");
		return -ENODEV;
	}
	LOG_INF("device is ready \n");

	/* Verify that the hardware device is ready to use */
	if (!pwm_is_ready_dt(&pwm_led0)) {
		LOG_ERR("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
		return 0;
	}

	LOG_INF("Starting breathing/fading effect on channel %d...\n", pwm_led0.channel);

	while (1) {
		ret = pwm_set_dt(&pwm_led0, FIXED_PERIOD, pulse_width);
		if (ret) {
			LOG_ERR("Error %d: failed to set pulse width\n", ret);
			return 0;
		}
		if (fading_up) {
			pulse_width += FADE_STEP_SIZE;
			if (pulse_width >= FIXED_PERIOD) {
				pulse_width = FIXED_PERIOD;
				fading_up = 0U;
			}
		} else {
			if (pulse_width >= FADE_STEP_SIZE) {
				pulse_width -= FADE_STEP_SIZE;
			} else {
				pulse_width = 0U;
			}
			if (pulse_width == 0U) {
				fading_up = 1U;
			}
		}
		// LOG_INF("pulse_width: %d, fading_up: %d", pulse_width, fading_up);

		ret = sensor_sample_fetch(p1);
		if (ret != 0) {
			return -1;
		}
		ret = sensor_channel_get(p1, SENSOR_CHAN_LIGHT, colors);
		if (ret != 0) {
			return -1;
		}
		double red = sensor_value_to_double(&colors[0]);
		double green = sensor_value_to_double(&colors[1]);
		double blue = sensor_value_to_double(&colors[2]);
		double clear = sensor_value_to_double(&colors[3]);

		LOG_PRINTK(">C:%.2f,R:%.2f,G:%.2f,B:%.2f\n", clear, red, green, blue);

		k_msleep(FADE_DELAY_MS);
	}
	return 0;
}
