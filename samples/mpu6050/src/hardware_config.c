#include <tmi/api/imu.h>
#include <zephyr/logging/log.h>

extern const tmi_imu_api_t mpu6050_api;
#define IMU0_API mpu6050_api

tmi_imu_config_t imu0_config = {
	.bus = DEVICE_DT_GET(DT_ALIAS(i2c0)),
	.addr = 0x68,
	.accel_fs_mG = 2000,
	.gyro_fs_dps = 250,
};

tmi_imu_data_t imu0_data = {
	.accel_iir = {.x = 0, .y = 0, .z = 0},
	.gyro_iir = {.x = 0, .y = 0, .z = 0},
};

struct device imu0 = {
	.config = &imu0_config,
	.api = &IMU0_API,
	.data = &imu0_data,
};
