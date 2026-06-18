#include <stdint.h>
#include <zephyr/kernel.h>
#include "mpu6050_defs.h"

typedef union {
	uint8_t bytes[6];
	struct {
		int16_t x;
		int16_t y;
		int16_t z;
	};
} mpu6050_accel_t;

typedef union {
	uint8_t bytes[6];
	struct {
		int16_t x;
		int16_t y;
		int16_t z;
	};
} mpu6050_gyro_t;

typedef struct {
	const struct device *i2c;
	const uint8_t i2c_addr;
	mpu6050_gyro_fs_t gyro_fs;
	mpu6050_accel_fs_t accel_fs;
} mpu6050_config_t;

typedef struct {
	mpu6050_accel_t accel_filtered;
	mpu6050_gyro_t gyro_filtered;
} mpu6050_data_t;

typedef struct {
	mpu6050_config_t config;
	mpu6050_data_t data;
} mpu6050_device_t;

int mpu6050_init(mpu6050_device_t *dev);
int mpu6050_check_whoami(mpu6050_device_t *dev);
int mpu6050_get_accel(mpu6050_device_t *dev, mpu6050_accel_t *val);
int mpu6050_get_gyro(mpu6050_device_t *dev, mpu6050_gyro_t *val);
int mpu6050_get_temp_mC(mpu6050_device_t *dev, int16_t *tmp);
int mpu6050_set_accel_fs(mpu6050_device_t *dev, mpu6050_accel_fs_t range);
int mpu6050_set_gyro_fs(mpu6050_device_t *dev, mpu6050_gyro_fs_t range);
