/**
 * @file mpu6050_defs.h
 * @brief
 * @details
 */

/**
 * @brief Full scale gyro conf selections
 */
typedef enum
{
    MPU6050_GYRO_CONF_FS_SEL_250_DPS = 0,
    MPU6050_GYRO_CONF_FS_SEL_500_DPS = 1,
    MPU6050_GYRO_CONF_FS_SEL_1000_DPS = 2,
    MPU6050_GYRO_CONF_FS_SEL_2000_DPS = 3,
    MPU6050_GYRO_CONF_FS_SEL_MAX = 4,
} mpu6050_gyro_fs_sel_t;