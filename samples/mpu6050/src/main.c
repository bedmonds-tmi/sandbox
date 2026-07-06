#include <zephyr/kernel.h>
#include <tmi/api/imu.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);

extern const tmi_imu_t imu0;

#define ERROR_LOOP(...)                                                                            \
	do {                                                                                       \
		while (1) {                                                                        \
			LOG_ERR(__VA_ARGS__);                                                      \
			k_msleep(1000);                                                            \
		}                                                                                  \
	} while (0)

int main(void)
{
	int ret = imu0.api->init(&imu0);

	tmi_imu_init(&imu0);
	if (ret != 0) {
		ERROR_LOOP("Init failed: %d", ret);
	}

	k_msleep(10);

	ret = tmi_imu_set_gyro_fs_dps(&imu0, 250);
	if (ret != 0) {
		ERROR_LOOP("Set Gyro FS failed: %d", ret);
	}

	ret = tmi_imu_set_accel_fs_mG(&imu0, 2000);
	if (ret != 0) {
		ERROR_LOOP("Set Accel FS failed: %d", ret);
	}

	while (1) {
		// Read and print temperature data
		int16_t temp = 0;
		tmi_imu_get_temp_mC(&imu0, &temp);
		LOG_PRINTK(">t:%02f\n", (double)temp / 1000.0);

		// // Read and print accelerometer data
		tmi_imu_vec3_t accel;
		ret = tmi_imu_get_accel(&imu0, &accel);
		if (ret != 0) {
			LOG_ERR("Get Accel failed: %d", ret);
		}

		LOG_PRINTK(">ax:%d,ay:%d,az:%d\n", accel.x, accel.y, accel.z);
		LOG_PRINTK(">axf:%d,ayf:%d,azf:%d\n", imu0.data.accel_iir.x, imu0.data.accel_iir.y,
			   imu0.data.accel_iir.z);

		// // Read and print gyroscope data
		tmi_imu_vec3_t gyro;
		ret = tmi_imu_get_gyro(&imu0, &gyro);
		if (ret != 0) {
			LOG_ERR("Get Gyro failed: %d", ret);
		}

		LOG_PRINTK(">gx:%d,gy:%d,gz:%d\n", gyro.x, gyro.y, gyro.z);
		LOG_PRINTK(">gxf:%d,gyf:%d,gzf:%d\n", imu0.data.gyro_iir.x, imu0.data.gyro_iir.y,
			   imu0.data.gyro_iir.z);

		k_msleep(500);
	}
}
