#include <zephyr/kernel.h>
#include <custom_drivers/mpu6050/mpu6050.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);

static mpu6050_device_t imu0 = {
	.config =
		{
			.i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0)),
			.i2c_addr = MPU6050_I2C_ADDR0,
			.gyro_fs = MPU6050_GYRO_CONF_FS_1000_DPS,
			.accel_fs = MPU6050_ACCEL_CONF_FS_16_G,
		},
};

#define ERROR_LOOP(...)                                                                            \
	do {                                                                                       \
		while (1) {                                                                        \
			LOG_ERR(__VA_ARGS__);                                                      \
			k_msleep(1000);                                                            \
		}                                                                                  \
	} while (0)

int main(void)
{
	int ret = mpu6050_init(&imu0);
	if (ret != 0) {
		ERROR_LOOP("Init failed: %d", ret);
	}

	k_msleep(10);

	ret = mpu6050_check_whoami(&imu0);
	if (ret != 0) {
		ERROR_LOOP("WHOAMI failed: %d", ret);
	}

	ret = mpu6050_set_gyro_fs(&imu0, MPU6050_GYRO_CONF_FS_250_DPS);
	if (ret != 0) {
		ERROR_LOOP("Set Gyro FS failed: %d", ret);
	}

	ret = mpu6050_set_accel_fs(&imu0, MPU6050_ACCEL_CONF_FS_2_G);
	if (ret != 0) {
		ERROR_LOOP("Set Accel FS failed: %d", ret);
	}

	while (1) {
		// Read and print temperature data
		int16_t temp = 0;
		mpu6050_get_temp_mC(&imu0, &temp);
		LOG_PRINTK(">t:%02f\n", (double)temp / 1000.0);

		// Read and print accelerometer data
		mpu6050_accel_t accel;
		ret = mpu6050_get_accel(&imu0, &accel);
		if (ret != 0) {
			LOG_ERR("Get Accel failed: %d", ret);
		}

		LOG_PRINTK(">ax:%d,ay:%d,az:%d\n", accel.x, accel.y, accel.z);
		LOG_PRINTK(">axf:%d,ayf:%d,azf:%d\n", imu0.data.accel_filtered.x,
			   imu0.data.accel_filtered.y, imu0.data.accel_filtered.z);

		// Read and print gyroscope data
		mpu6050_gyro_t gyro;
		ret = mpu6050_get_gyro(&imu0, &gyro);
		if (ret != 0) {
			LOG_ERR("Get Gyro failed: %d", ret);
		}

		LOG_PRINTK(">gx:%d,gy:%d,gz:%d\n", gyro.x, gyro.y, gyro.z);
		LOG_PRINTK(">gxf:%d,gyf:%d,gzf:%d\n", imu0.data.gyro_filtered.x,
			   imu0.data.gyro_filtered.y, imu0.data.gyro_filtered.z);

		k_msleep(500);
	}
}
