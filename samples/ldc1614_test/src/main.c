#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

/* I2C device - adjust DT_NODELABEL to match your board's device tree */
#define I2C_NODE DT_NODELABEL(i2c0)

/* LDC1614 I2C Address: 0x2A when ADDR pin is low, 0x2B when high[cite: 5] */
#define LDC1614_ADDR 0x2A

/* LDC1614 Registers referenced from ldc1614.pdf */
#define REG_DATA0_MSB 0x00
#define REG_DATA0_LSB 0x01
#define REG_CONFIG    0x1A
#define REG_MANUF_ID  0x7E
#define REG_DEVICE_ID 0x7F

static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

/*
 * The LDC1614 uses 16-bit registers accessed via an 8-bit pointer.
 * A repeated start is used to access the second byte, reading the MSB first[cite: 14].
 */
int ldc1614_read_reg(uint8_t reg, uint16_t *val)
{
	uint8_t buf[2] = {0};
	int ret = i2c_write(i2c_dev, &reg, 1, LDC1614_ADDR);
	if (ret != 0) {
		printk("Failed to write register address 0x%02X: %d\n", reg, ret);
		return ret;
	}
	ret = i2c_read(i2c_dev, buf, 2, LDC1614_ADDR);
	if (ret != 0) {
		printk("Failed to read register 0x%02X: %d\n", reg, ret);
		return ret;
	}
	*val = (buf[0] << 8) | buf[1];
	return 0;
}
int ldc1614_write_reg(uint8_t reg, uint16_t val)
{
	uint8_t buf[3] = {reg, (val >> 8) & 0xFF, val & 0xFF};
	int ret = i2c_write(i2c_dev, buf, 3, LDC1614_ADDR);
	if (ret != 0) {
		printk("Failed to write register 0x%02X: %d\n", reg, ret);
		return ret;
	}
	return 0;
}

int main()
{
	uint16_t manuf_id, dev_id, data_msb, data_lsb;

	if (!device_is_ready(i2c_dev)) {
		printk("I2C bus not ready\n");
		return 0;
	}
	printk("I2C bus ready, starting LDC1614 test...\n");

	printk("Initializing LDC1614 on I2C bus %s, address 0x%02X\n", i2c_dev->name, LDC1614_ADDR);

	while (1) {
		ldc1614_read_reg(REG_MANUF_ID, &manuf_id);
		ldc1614_read_reg(REG_DEVICE_ID, &dev_id);
		printk("Manufacturer ID: 0x%04X (Expected: 0x5449)[cite: 34]\n", manuf_id);
		printk("Device ID: 0x%04X (Expected: 0x3055)[cite: 34]\n", dev_id);

		ldc1614_read_reg(REG_DATA0_MSB, &data_msb);
		ldc1614_read_reg(REG_DATA0_LSB, &data_lsb);
		printk("Data0 MSB: 0x%04X\n", data_msb);
		printk("Data0 LSB: 0x%04X\n", data_lsb);

		k_msleep(500);
	}
	return 0;
}
