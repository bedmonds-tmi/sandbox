#include <zephyr/kernel.h>
#include <custom_drivers/mpu6050/mpu6050.h>
#include <stdint.h>

int main(void)
{
	int ret = 0;

	ret = mpu6050_init();
	if (ret != 0)
	{
		return ret;
	}
	k_msleep(1000);
	uint8_t x;
	ret = mpu6050_whoami(&x);
	if (ret != 0)
	{
		return ret;
	}
	printk("WHOAMI: 0x%02X\n", x);

	while (1)
	{
		ret = mpu6050_accelerometer_scale_range(0);
		if (ret != 0)
		{
			return ret;
		}
		accel_t accel_num;
		ret = mpu6050_get_accel(&accel_num);
		if (ret != 0)
		{
			return ret;
		}
		printk(">ax:%d g, ay:%d g ,az:%d g \n", accel_num.x / 16384, accel_num.y / 16384, accel_num.z / 16384);
		k_msleep(100);

		ret = mpu6050_gyroscope_scale_range(0);
		if (ret != 0)
		{
			return ret;
		}
		gyro_t gyro_num;
		ret = mpu6050_get_gyro(&gyro_num);
		if (ret != 0)
		{
			return ret;
		}
		printk(">gx:%d deg, gy:%d deg ,gz:%d deg \n", gyro_num.x / 131, gyro_num.y / 131, gyro_num.z / 131);
		k_msleep(100);
	}
}
