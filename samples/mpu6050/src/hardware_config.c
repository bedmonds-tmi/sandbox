#include <tmi/api/imu.h>

extern const tmi_imu_api_t mpu6050_api;
#define IMU0_API mpu6050_api

tmi_imu_t imu0 = {
	.config =
		{
			.bus = DEVICE_DT_GET(DT_ALIAS(i2c0)),
			.addr = 0x68,
			.accel_fs_mG = 2000,
			.gyro_fs_dps = 250,
		},
	.api = &IMU0_API,
};
