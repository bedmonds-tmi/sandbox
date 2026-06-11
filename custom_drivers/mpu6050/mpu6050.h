#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

typedef struct
{
    int16_t x, y, z;
} accel_t;

typedef struct
{
    int16_t x, y, z;
} gyro_t;

/**
 * @brief
 * @param
 * @param
 * @returns
 */
int mpu6050_init(void);

/**
 * @brief
 * @returns
 */
int mpu6050_whoami(uint8_t *val);
/**
 * @brief
 * @param
 * @param
 * @returns
 */
int mpu6050_get_accel(accel_t *val);
/**
 * @brief
 * @param
 * @param
 * @returns
 */
int mpu6050_get_gyro(gyro_t *val);
/**
 * @brief
 * @param
 * @param
 * @returns
 */
int mpu6050_get_temp_C(float *tmp);
/**
 * @brief
 * @param
 * @param
 * @returns
 */
int mpu6050_accelerometer_scale_range(int mode);
/**
 * @brief
 * @param
 * @param
 * @returns
 */
int mpu6050_gyroscope_scale_range(int mode);
