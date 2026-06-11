#include "mpu6050.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#define I2C_NODE DT_NODELABEL(i2c0)

#define WHOAMI_VAL 0x68

#define MPU6050_ACCEL_XOUT 0x3C
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_REG_WHOAMI 0x75
#define MPU6050_REG_USER_CTRL 0x6A
#define MPU6050_SELF_TEST_X 0x0D
#define MPU6050_SELF_TEST_Y 0x0E
#define MPU6050_SELF_TEST_Z 0x0F
#define MPU6050_SELF_TEST_A 0x10
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_SMPLRT_DIV 0x19
#define MPU6050_CONFIG 0x1A
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_MOT_THR 0x1F
#define MPU6050_FIFO_EN 0x23
#define MPU6050_I2C_MST_CTRL
#define MPU6050_I2C_SLV0_ADDR
#define MPU6050_I2C_SLV0_REG
#define MPU6050_I2C_SLV0_CTRL
#define MPU6050_I2C_SLV1_ADDR
#define MPU6050_I2C_SLV1_REG
#define MPU6050_I2C_SLV1_CTRL
#define MPU6050_I2C_SLV2_ADDR
#define MPU6050_I2C_SLV2_REG
#define MPU6050_I2C_SLV2_CTRL
#define MPU6050_I2C_SLV3_ADDR
#define MPU6050_I2C_SLV3_REG
#define MPU6050_I2C_SLV3_CTRL
#define MPU6050_I2C_SLV4_ADDR
#define MPU6050_I2C_SLV4_REG
#define MPU6050_I2C_SLV4_DO
#define MPU6050_I2C_SLV4_CTRL

long temp_calibration_value = 0;
const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

static int read_reg(uint8_t reg, uint8_t *val)
{
	return i2c_write_read(i2c_dev, MPU6050_I2C_ADDR, &reg, 1, val, 1);
}
static int write_reg(uint8_t reg, uint8_t val)
{
	return i2c_reg_write_byte(i2c_dev, MPU6050_I2C_ADDR, reg, val);
}
static int write_to_bit(uint8_t reg, uint8_t val, uint8_t bit_pos)
{
	uint8_t tmp;

	int ret = read(reg, &tmp);
	if (ret != 0)
	{
		printk("Error: %d\n", ret);
		return ret;
	}
	tmp &= ~(1 << bit_pos);					  // Clear the target bit
	tmp |= (val << bit_pos) & (1 << bit_pos); // Set the target bit based on val
	return write(reg, tmp);
}

int mpu6050_init(void)
{
	printk("mpu6050 init\n");
	return 0;
}
int mpu6050_whoami(uint8_t *val)
{
	int ret;
	uint8_t x;
	ret = read(MPU6050_REG_WHOAMI, &x);
	if (ret != 0)
	{
		return ret;
	}
	if (x != WHOAMI_VAL)
	{
		return -EINVAL;
	}
	*val = x;
	return 0;
}

int mpu6050_get_accel(accel_t *val)
{
	uint8_t val0, val1;

	read(0x3B, &val0);
	read(0x3C, &val1);
	val->x = ((int16_t)val0 << 8) | val1;

	read(0x3D, &val0);
	read(0x3E, &val1);
	val->y = ((int16_t)val0 << 8) | val1;

	read(0x3F, &val0);
	read(0x40, &val1);
	val->z = ((int16_t)val0 << 8) | val1;
	return 0;
}

int mpu6050_get_gyro(gyro_t *val)
{
	uint8_t val0, val1;

	read(0x43, &val0);
	read(0x44, &val1);
	val->x = ((int16_t)val0 << 8) | val1;

	read(0x45, &val0);
	read(0x46, &val1);
	val->y = ((int16_t)val0 << 8) | val1;

	read(0x47, &val0);
	read(0x48, &val1);
	val->z = ((int16_t)val0 << 8) | val1;
	return 0;
}

/**
 * @brief Sets the scale range for gyroscope.
 * @param[in] mode - integer value of range mode
 * @details
 * 			GYROSCOPE SENSITIVITY
 * 			Full-Scale Range
 * 			FS_SEL=0  ±250  º/s
 * 			FS_SEL=1  ±500  º/s
 * 			FS_SEL=2  ±1000  º/s
 * 			FS_SEL=3  ±2000  º/s
 * 			Gyroscope ADC Word Length   16  bits
 * 			Sensitivity Scale Factor
 * 			FS_SEL=0  131  LSB/(º/s)
 * 			FS_SEL=1  65.5  LSB/(º/s)
 * 			FS_SEL=2  32.8  LSB/(º/s)
 * 			FS_SEL=3  16.4  LSB/(º/s)
 * @retval
 * 			- 0 on success
 * 			- ERANGE
 */
int mpu6050_gyroscope_scale_range(int mode)
{
	if (0 > mode || mode > 3)
	{
		return -ERANGE;
	}
	switch (mode)
	{
	case 0:
		write(MPU6050_GYRO_CONFIG, 0x00);
		break;
	case 1:
		write(MPU6050_GYRO_CONFIG, 0x08);
		break;
	case 2:
		write(MPU6050_GYRO_CONFIG, 0x10);
		break;
	case 3:
		write(MPU6050_GYRO_CONFIG, 0x18);
		break;
	}
	return 0;
}
/**
 * @brief Sets the scale range for Acellerometer.
 * @param[in] mode - integer value of range mode
 * @details
 * 			Acellerometer SENSITIVITY
 * 			Full-Scale Range
 * 			FS_SEL=0  ±2  º/s
 * 			FS_SEL=1  ±4  º/s
 * 			FS_SEL=2  ±8  º/s
 * 			FS_SEL=3  ±16  º/s
 * 			acellerometer ADC Word Length   16  bits
 * 			Sensitivity Scale Factor
 * 			FS_SEL=0  16384  LSB/(º/s)
 * 			FS_SEL=1  8192  LSB/(º/s)
 * 			FS_SEL=2  4096  LSB/(º/s)
 * 			FS_SEL=3  2048  LSB/(º/s)
 * @retval
 * 			- 0 on success
 * 			- ERANGE
 */
int mpu6050_accelerometer_scale_range(int mode)
{
	if (0 > mode || mode > 3)
	{
		return -errno;
	}
	switch (mode)
	{
	case 0:
		write(MPU6050_ACCEL_CONFIG, 0x00);
		break;
	case 1:
		write(MPU6050_ACCEL_CONFIG, 0x08);
		break;
	case 2:
		write(MPU6050_ACCEL_CONFIG, 0x10);
		break;
	case 3:
		write(MPU6050_ACCEL_CONFIG, 0x18);
		break;

		return 0;
	}
}

int mpu6050_get_temp_C(float *tmp_C)
{
	uint8_t val0, val1;
	read(0x41, &val0);
	read(0x42, &val1);
	int16_t val = ((int16_t)val0 << 8) | val1;
	*tmp_C = (float)val / 340.0;
	*tmp_C += 36.53;
	return 0;
}
