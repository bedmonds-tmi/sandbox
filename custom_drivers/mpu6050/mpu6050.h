#include <stdint.h>
#include <zephyr/kernel.h>

/**
 * @brief struct with 3 16 bit intagers
 */
typedef struct
{
    int16_t x, y, z;
} accel_t;

/**
 * @brief struct with 3 16 bit intagers
 */
typedef struct
{
    int16_t x, y, z;
} gyro_t;

/**
 * @brief ensures main.c can properly utilize functions from header file
 * @returns  -0 upon sucess
 *           -a negative number representing an Error message
 */
int mpu6050_init(void);

/**
 * @brief reads who am I register, this enusures that the read function is working and program can commmunicate with registers
 * @param[out] will update 8 bit unsigned integer with hexadecimal stored in the who am I register
 * @returns -0 upon sucess
 *          -neg upon failure with corrosponding message
 */
int mpu6050_whoami(uint8_t *val);
/**
 * @brief reads 6 registers 2 for each axis x,y and z and updates a accel_t type struct
 * @param[out] updates the 3 parameters of the acell_t object x,y and z
 * @returns -0 upon sucess
 *          -neg upon failure with corrosponding message
 */
int mpu6050_get_accel(accel_t *val);
/**
 * @brief reads 6 registers 2 for each axis x,y and z and updates a gyro_t type struct
 * @param[out] updates the 3 parameters of the gyro_t object x,y and z
 * @returns-0 upon sucess
 *          -neg upon failure with corrosponding message
 */
int mpu6050_get_gyro(gyro_t *val);
/**
 * @brief reads 6 registers 2 for each axis x,y and z and updates a gyro_t type struct
 * @param[out] updates the float type passed to it
 * @returns-0 upon sucess
 *          -neg upon failure with corrosponding message
 */
int mpu6050_get_temp_C(float *tmp);
/**
 * @brief selects one of 4 available ranges for the sensitivity of the accelerometer
 * @param[in] intager from 0 to for to select
 * @returns 0 upon sucess else a neg #
 */
int mpu6050_accelerometer_scale_range(int mode);
/**
 * @brief selects one of 4 available ranges for the sensitivity of the accelerometer
 * @param[in] intager from 0 to for to select
 * @returns 0 upon sucess else a neg #
 */
int mpu6050_gyroscope_scale_range(int mode);
