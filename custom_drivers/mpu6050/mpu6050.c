#include "mpu6050.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

int mpu6050_init(void)
{
	printk("mpu6050 init\n");
	return 0;
}
