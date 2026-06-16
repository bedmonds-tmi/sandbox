#include "mpu6050.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mpu6050);

#define MPU6050_REG_ACCEL_XOUTH  0x3B
#define MPU6050_REG_TEMP_OUTH    0x41
#define MPU6050_REG_GYRO_XOUTH   0x43
#define MPU6050_REG_WHOAMI       0x75
#define MPU6050_REG_USER_CTRL    0x6A
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_SELF_TEST_X  0x0D
#define MPU6050_REG_SELF_TEST_Y  0x0E
#define MPU6050_REG_SELF_TEST_Z  0x0F
#define MPU6050_REG_SELF_TEST_A  0x10
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_MOT_THR      0x1F
#define MPU6050_REG_FIFO_EN      0x23

#define MPU6050_ACCEL_DATA_LEN 6U
#define MPU6050_TEMP_DATA_LEN  2U
#define MPU6050_GYRO_DATA_LEN  6U

/**
 * GENMASK(3,4) is same as saying (0x03 << 3), i.e. bits 3 and 4 are flipped to 1 (0b00011000)
 * BIT(5) is same as (1 << 5), .i.e. bit 5 flipped to 1 (0b00100000)
 */
#define MPU6050_MASK_ACCEL_CONFIG_AFS_SEL GENMASK(4, 3)
#define MPU6050_MASK_ACCEL_CONFIG_ZA_ST   BIT(5)
#define MPU6050_MASK_ACCEL_CONFIG_YA_ST   BIT(6)
#define MPU6050_MASK_ACCEL_CONFIG_XA_ST   BIT(7)

#define MPU6050_MASK_GYRO_CONFIG_FS_SEL GENMASK(4, 3)
#define MPU6050_MASK_GYRO_CONFIG_ZG_ST  BIT(5)
#define MPU6050_MASK_GYRO_CONFIG_YG_ST  BIT(6)
#define MPU6050_MASK_GYRO_CONFIG_XG_ST  BIT(7)

#define MPU6050_MASK_PWR_MGMT_1_SLEEP BIT(6)

#define CHECK_NULL_PTR(ptr, ...)                                                                   \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR(__VA_ARGS__);                                                      \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0);

static int read_reg(const mpu6050_device_t *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	return i2c_write_read(dev->config.i2c, dev->config.i2c_addr, &reg, 1, val, len);
}

static int write_reg(const mpu6050_device_t *dev, uint8_t reg, uint8_t val)
{
	return i2c_reg_write_byte(dev->config.i2c, dev->config.i2c_addr, reg, val);
}

static int write_mask(mpu6050_device_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t tmp;

	int ret = read_reg(dev, reg, &tmp, sizeof(tmp));
	if (ret != 0) {
		LOG_ERR("Error: %d", ret);
		return ret;
	}

	tmp &= ~mask;                 // Clear the target bit
	tmp |= FIELD_PREP(mask, val); // Set the target bit based on val

	return write_reg(dev, reg, tmp);
}

/**
 * @brief Initialize mpu6050 driver
 * @param[in] dev - device pointer
 * @retval 0 on success, errno on failure
 */
int mpu6050_init(mpu6050_device_t *dev)
{
	if (dev == NULL) {
		LOG_ERR("Null pointer to device.");
		return -EINVAL;
	}

	if (dev->config.i2c == NULL) {
		LOG_ERR("Null pointer to i2c bus.");
		return -EINVAL;
	}

	if (dev->config.i2c_addr != MPU6050_I2C_ADDR0 &&
	    dev->config.i2c_addr != MPU6050_I2C_ADDR1) {
		LOG_ERR("Invalid I2C address: 0x%02X", dev->config.i2c_addr);
		return -EINVAL;
	}

	int ret = mpu6050_check_whoami(dev);
	if (ret != 0) {
		return -ERANGE;
	}

	// Exit sleep mode by clearing sleep bit in PWR_MGMT_1
	ret = write_mask(dev, MPU6050_REG_PWR_MGMT_1, MPU6050_MASK_PWR_MGMT_1_SLEEP, 0);
	if (ret != 0) {
		LOG_ERR("Failed to write to wake device: %d", ret);
		return ret;
	}

	ret = mpu6050_set_gyro_fs(dev, dev->config.gyro_fs);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set gyro fs (Err %d).", ret);
		return ret;
	}

	ret = mpu6050_set_accel_fs(dev, dev->config.accel_fs);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set accel fs (Err %d).", ret);
		return ret;
	}

	// Initialize filters
	mpu6050_accel_t accel;
	mpu6050_gyro_t gyro;

	ret = mpu6050_get_accel(dev, &accel);
	if (ret != 0) {
		return ret;
	}

	ret = mpu6050_get_gyro(dev, &gyro);
	if (ret != 0) {
		return ret;
	}

	dev->data.accel_filtered = accel;
	dev->data.gyro_filtered = gyro;

	LOG_INF("Initialized");
	return 0;
}

/**
 * @brief Read who am I register
 * @details Enusures that the read function is working and program can commmunicate with registers
 * @param[in] dev - pointer to device
 * @returns - 0 upon sucess
 *          - errno upon failure with corrosponding message
 */
int mpu6050_check_whoami(mpu6050_device_t *dev)
{
	CHECK_NULL_PTR(dev, "Null dev pointer passed to WHOAMI.");

	uint8_t x;
	int ret = read_reg(dev, MPU6050_REG_WHOAMI, &x, sizeof(x));
	if (ret != 0) {
		LOG_ERR("Read reg failed when reading WHOAMI reg: %d", ret);
		return ret;
	}

	/* WHOAMI value will be equal to the address. */
	if (x != dev->config.i2c_addr) {
		LOG_ERR("WHOAMI mismatch. Got 0x%02X, expected 0x%02X", x, dev->config.i2c_addr);
		return -ERANGE;
	}

	return 0;
}

/**
 * @brief IIR filter
 * @details
 * 		new_val = raw*alpha + filtered*(1 - alpha)
 * 						= raw*alpha + filtered - filter*alpha
 * 						= filtered + (raw-filtered)*alpha
 * @returns updated filtered value
 */
static int16_t iir_update(int16_t filtered, int16_t raw)
{
	int32_t diff = (int32_t)raw - filtered;

	// Shifting by 3 is the same as dividing diff by 8 (i.e. alpha of 1/8)
	int32_t new = filtered + (diff >> 3);

	return (int16_t)new;
}

int mpu6050_get_accel(mpu6050_device_t *dev, mpu6050_accel_t *raw)
{
	uint8_t tmp[6];
	read_reg(dev, MPU6050_REG_ACCEL_XOUTH, tmp, sizeof(tmp));

	// Big endian (high byte first, low byte second)
	raw->x = (int16_t)(((uint16_t)tmp[0] << 8) | tmp[1]);
	raw->y = (int16_t)(((uint16_t)tmp[2] << 8) | tmp[3]);

	// Can also use zephyr's built-in function
	raw->z = sys_get_be16(&tmp[4]);

	dev->data.accel_filtered.x = iir_update(dev->data.accel_filtered.x, raw->x);
	dev->data.accel_filtered.y = iir_update(dev->data.accel_filtered.y, raw->y);
	dev->data.accel_filtered.z = iir_update(dev->data.accel_filtered.z, raw->z);

	return 0;
}

int mpu6050_get_gyro(mpu6050_device_t *dev, mpu6050_gyro_t *raw)
{
	uint8_t tmp[6];
	read_reg(dev, MPU6050_REG_GYRO_XOUTH, tmp, sizeof(tmp));

	// Big endian (high byte first, low byte second)
	raw->x = (int16_t)(((uint16_t)tmp[0] << 8) | tmp[1]);
	raw->y = (int16_t)(((uint16_t)tmp[2] << 8) | tmp[3]);

	// Can also use zephyr's built-in function
	raw->z = sys_get_be16(&tmp[4]);

	dev->data.gyro_filtered.x = iir_update(dev->data.gyro_filtered.x, raw->x);
	dev->data.gyro_filtered.y = iir_update(dev->data.gyro_filtered.y, raw->y);
	dev->data.gyro_filtered.z = iir_update(dev->data.gyro_filtered.z, raw->z);

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
int mpu6050_set_gyro_fs(mpu6050_device_t *dev, mpu6050_gyro_fs_t range)
{
	if (range >= MPU6050_GYRO_CONF_FS_MAX && range < 0) {
		LOG_ERR("Invalid gyro fs range.");
		return -ERANGE;
	}

	int ret = write_mask(dev, MPU6050_REG_GYRO_CONFIG, MPU6050_MASK_GYRO_CONFIG_FS_SEL,
			     (uint8_t)range);
	if (ret != 0) {
		return ret;
	}

	dev->config.gyro_fs = range;

	return 0;
}
/**
 * @brief Sets the scale range for Acellerometer.
 * @param[in] mode - integer value of range mode
 * @details
 * 			Accelerometer SENSITIVITY
 * 			Full-Scale Range
 * 			FS_SEL=0  ±2  º/s
 * 			FS_SEL=1  ±4  º/s
 * 			FS_SEL=2  ±8  º/s
 * 			FS_SEL=3  ±16  º/s
 * 			Accelerometer ADC Word Length   16  bits
 * 			Sensitivity Scale Factor
 * 			FS_SEL=0  16384  LSB/(º/s)
 * 			FS_SEL=1  8192  LSB/(º/s)
 * 			FS_SEL=2  4096  LSB/(º/s)
 * 			FS_SEL=3  2048  LSB/(º/s)
 * @retval
 * 			- 0 on success
 * 			- ERANGE
 */
int mpu6050_set_accel_fs(mpu6050_device_t *dev, mpu6050_accel_fs_t range)
{
	if (range >= MPU6050_ACCEL_CONF_FS_MAX && range < 0) {
		LOG_ERR("Invalid accel fs range.");
		return -ERANGE;
	}

	int ret = write_mask(dev, MPU6050_REG_ACCEL_CONFIG, MPU6050_MASK_ACCEL_CONFIG_AFS_SEL,
			     (uint8_t)range);
	if (ret != 0) {
		return ret;
	}

	dev->config.accel_fs = range;

	return 0;
}

/**
 * @brief Sends milliCelsius (Celsius * 1000)
 * @details Temperature in degrees C = (TEMP_OUT Register Value as a signed quantity)/340 + 36.53
 */
int mpu6050_get_temp_mC(mpu6050_device_t *dev, int16_t *tmp_mC)
{
	uint8_t tmp[2];
	read_reg(dev, MPU6050_REG_TEMP_OUTH, tmp, sizeof(tmp));

	int16_t raw = sys_get_be16(&tmp[0]);
	*tmp_mC = (int32_t)raw * 1000 / 340;
	*tmp_mC += 36530;

	return 0;
}
