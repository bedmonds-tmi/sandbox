#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

#define I2C_NODE DT_NODELABEL(i2c0)

// #define LED0_NODE DT_ALIAS(led_en)
// static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

int tcs3400_read(uint8_t reg, uint8_t *val)
{
	return i2c_write_read(i2c_dev, TCS3400, &reg, 1, val, 1);
}

int tcs3400_write(uint8_t reg, uint8_t val)
{
	return i2c_reg_write_byte(i2c_dev, TCS3400, reg, val);
}

int main(void)
{
	int ret;

	if (!device_is_ready(i2c_dev)) {
		printk("I2C device not ready\n");
		return 0;
	}
	/*Initializing registers*/
	tcs3400_write(SYSM_CTRL, 3);
	tcs3400_write(INT_FLAG, 0);
	tcs3400_write(WAIT_TIME, 255);
	tcs3400_write(TCS_GAIN, 3);
	tcs3400_write(TCS_TIME, 255);

	// ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	while (1) {
		uint8_t x, val0, val1, val2, val3, val4, val5;

		tcs3400_read(RCH_DATA_H, &val0);
		tcs3400_read(RCH_DATA_L, &val1);
		tcs3400_read(GCH_DATA_H, &val2);
		tcs3400_read(GCH_DATA_L, &val3);
		tcs3400_read(BCH_DATA_H, &val4);
		tcs3400_read(BCH_DATA_L, &val5);

		// ret = gpio_pin_toggle_dt(&led);

		int16_t red = ((int16_t)val0 << 8) | val1;
		printk(">red:%d\n", red);
		int16_t green = ((int16_t)val2 << 8) | val3;
		printk(">green:%d\n", green);
		int16_t blue = ((int16_t)val4 << 4) | val5;
		printk(">blue:%d\n", blue);

		ret = tcs3400_read(ID, &x);
		if (ret == 0) {
			printk("ID: 0x%02X\n", x);
		} else {
			printk("I2C read failed: %d\n", ret);
		}
		k_msleep(1000);
	}
}
