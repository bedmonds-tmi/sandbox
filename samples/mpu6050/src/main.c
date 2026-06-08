#include <zephyr/kernel.h>
#include <custom_drivers/mpu6050/mpu6050.h>

int main(void)
{
	while (1)
	{
		mpu6050_init();
		k_msleep(1000);
	}
}
