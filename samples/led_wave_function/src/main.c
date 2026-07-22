#include <zephyr/kernel.h>
#include <tmi/api/pressure.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(apps);

#define MAX_SAMPLES        200
#define SAMPLE_INTERVAL_US 22500

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(p1));

static const struct gpio_dt_spec led_0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led_1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led_2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led_3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

int main(void)
{
	int ret = tmi_pressure_init(p1);
	if (ret != 0) {
		LOG_ERR("Init failed: %d\n", ret);
		return ret;
	}

	if (!gpio_is_ready_dt(&led_0) && !gpio_is_ready_dt(&led_1) && !gpio_is_ready_dt(&led_2) &&
	    !gpio_is_ready_dt(&led_3)) {
		return 0;
	}

	gpio_pin_configure_dt(&led_0, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&led_1, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&led_2, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&led_3, GPIO_OUTPUT_ACTIVE);

	uint32_t start_time = k_uptime_get_32();
	uint32_t elapsed = 0;
	int sample_count = 0;
	LOG_INF("DATA_START\n");
	LOG_INF("Time(ms),Pressure(Pa)\n");

	while (sample_count < MAX_SAMPLES) {
		if (1) {
			// Wave forward: 0 -> 1 -> 2 -> 3
			ret = gpio_pin_toggle_dt(&led_0);
			k_msleep(300);
			ret = gpio_pin_toggle_dt(&led_1);
			k_msleep(300);
			ret = gpio_pin_toggle_dt(&led_2);
			k_msleep(300);
			ret = gpio_pin_toggle_dt(&led_3);
			k_msleep(300);

			// Wave backward: 2 -> 1 -> 0
			ret = gpio_pin_toggle_dt(&led_3);
			k_msleep(300);
			ret = gpio_pin_toggle_dt(&led_2);
			k_msleep(300);
			ret = gpio_pin_toggle_dt(&led_1);
			k_msleep(300);
			ret = gpio_pin_toggle_dt(&led_0);
			k_msleep(300);
		} else if (0) {
			LOG_INF("flow is too High!!");
			gpio_pin_toggle_dt(&led_0);
			gpio_pin_toggle_dt(&led_1);
			gpio_pin_toggle_dt(&led_2);
			gpio_pin_toggle_dt(&led_3);
			k_msleep(200);
			gpio_pin_toggle_dt(&led_0);
			gpio_pin_toggle_dt(&led_1);
			gpio_pin_toggle_dt(&led_2);
			gpio_pin_toggle_dt(&led_3);
		}
		sample_count++;
	}

	/* Use microsecond sleep to accurately hit the 22.5ms intervals */
	k_usleep(SAMPLE_INTERVAL_US);
	LOG_INF("DATA_END\n");

	while (1) {
		k_msleep(1000);
	}
}
