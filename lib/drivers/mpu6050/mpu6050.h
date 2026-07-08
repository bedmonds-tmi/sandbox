/**
 * @file mpu6050_defs.h
 * @brief
 * @details
 */

#include <zephyr/drivers/i2c.h>
#include <tmi/api/imu.h>

#define MPU6050_I2C_ADDR0 0x68
#define MPU6050_I2C_ADDR1 0x69

/**
 * @brief Full scale accel conf selections
 */
typedef enum {
	MPU6050_ACCEL_CONF_FS_2_G = 0,
	MPU6050_ACCEL_CONF_FS_4_G = 1,
	MPU6050_ACCEL_CONF_FS_8_G = 2,
	MPU6050_ACCEL_CONF_FS_16_G = 3,
	MPU6050_ACCEL_CONF_FS_MAX = 4,
} mpu6050_accel_fs_t;

/**
 * @brief Full scale gyro conf selections
 */
typedef enum {
	MPU6050_GYRO_CONF_FS_250_DPS = 0,
	MPU6050_GYRO_CONF_FS_500_DPS = 1,
	MPU6050_GYRO_CONF_FS_1000_DPS = 2,
	MPU6050_GYRO_CONF_FS_2000_DPS = 3,
	MPU6050_GYRO_CONF_FS_MAX = 4,
} mpu6050_gyro_fs_t;

typedef struct {
	struct i2c_dt_spec i2c;
	uint8_t alpha_div;    /** IIR filter alpha divisor. */
	uint16_t accel_fs_mG;
	uint16_t gyro_fs_dps;
} mpu6050_config_t;

typedef struct {
	mpu6050_config_t config; /** Device configuration. */
	tmi_imu_vec3_t accel_iir; /** Accelerometer raw IIR filtered data. */
	tmi_imu_vec3_t gyro_iir;  /** Gyroscope raw IIR filtered data. */
} mpu6050_data_t;