#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "mpu6050.h"

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
 * GENMASK(4, 3) is the same as (0x03 << 3), so bits 3 and 4 are set
 * (0b00011000). BIT(5) is the same as (1 << 5), so bit 5 is set
 * (0b00100000).
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

#define CHECK_NULL_PTR(ptr)                                                                        \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR("%s: null pointer: " #ptr, __func__);                              \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

static int read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;
	return i2c_write_read_dt(&cfg->i2c, &reg, 1, val, len);
}

static int write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;
	return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

static int write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
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
 * @brief Verify communication with the MPU6050.
 *
 * @details Reads the WHOAMI register and checks that it matches the configured
 * I2C address.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 *
 * @retval 0 The expected WHOAMI value was read.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from the register read operation on failure.
 */
static int mpu6050_check_whoami(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;

	uint8_t x;
	int ret = read_reg(dev, MPU6050_REG_WHOAMI, &x, sizeof(x));
	if (ret != 0) {
		LOG_ERR("Read reg failed when reading WHOAMI reg: %d", ret);
		return ret;
	}

	/* WHOAMI value will be equal to the address. */
	if (x != cfg->i2c.addr) {
		LOG_ERR("WHOAMI mismatch. Got 0x%02X, expected 0x%02X", x, cfg->i2c.addr);
		return -ERANGE;
	}

	return 0;
}

/**
 * @brief Read the accelerometer sample.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[out] raw Destination for the raw accelerometer sample. Must not be
 * NULL.
 *
 * @details Updates the stored accelerometer IIR filter state after reading the
 * raw X, Y, and Z samples.
 *
 * @retval 0 Accelerometer sample was read successfully.
 * @retval -EINVAL If @p dev or @p raw is NULL.
 */
static int mpu6050_get_accel(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	uint8_t tmp[6];
	int ret = read_reg(dev, MPU6050_REG_ACCEL_XOUTH, tmp, sizeof(tmp));
	if (ret != 0) {
		return ret;
	}

	// Big endian (high byte first, low byte second)
	int16_t x = sys_get_be16(&tmp[0]);
	int16_t y = sys_get_be16(&tmp[2]);
	int16_t z = sys_get_be16(&tmp[4]);

	data->accel[0].val1 = x % 1000000;
	data->accel[0].val2 = x / 1000000;

	data->accel[1].val1 = y % 1000;
	data->accel[1].val2 = y / 1000;

	data->accel[2].val1 = z % 1000;
	data->accel[2].val2 = z / 1000;

	return 0;
}

/**
 * @brief Read the gyroscope sample.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[out] raw Destination for the raw gyroscope sample. Must not be NULL.
 *
 * @details Updates the stored gyroscope IIR filter state after reading the raw
 * X, Y, and Z samples.
 *
 * @retval 0 Gyroscope sample was read successfully.
 * @retval -EINVAL If @p dev or @p raw is NULL.
 */
static int mpu6050_get_gyro(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	uint8_t tmp[6];
	read_reg(dev, MPU6050_REG_GYRO_XOUTH, tmp, sizeof(tmp));

	int16_t x, y, z;

	// Big endian (high byte first, low byte second)
	x = sys_get_be16(&tmp[0]);
	y = sys_get_be16(&tmp[2]);
	z = sys_get_be16(&tmp[4]);

	data->gyro[0].val1 = x % 1000000;
	data->gyro[0].val2 = x / 1000000;

	data->gyro[1].val1 = x % 1000000;
	data->gyro[1].val2 = x / 1000000;

	data->gyro[2].val1 = x % 1000000;
	data->gyro[2].val2 = x / 1000000;

	return 0;
}

static mpu6050_gyro_fs_t mpu6050_dps_to_fs_sel(int32_t dps)
{
	if (dps < 250) {
		return MPU6050_GYRO_CONF_FS_250_DPS;
	} else if (dps < 500) {
		return MPU6050_GYRO_CONF_FS_500_DPS;
	} else if (dps < 1000) {
		return MPU6050_GYRO_CONF_FS_1000_DPS;
	} else if (dps < 2000) {
		return MPU6050_GYRO_CONF_FS_2000_DPS;
	} else {
		return MPU6050_GYRO_CONF_FS_MAX;
	}
}

/**
 * @brief Set the gyroscope full-scale range.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[in] dps Requested gyroscope range in degrees per second.
 *
 * @details
 * The selected FS_SEL value is the first range that can cover @p dps. Values
 * above 2000 dps are clamped to the 2000 dps range.
 *
 * - FS_SEL=0: +/-250 dps, 131 LSB/dps
 * - FS_SEL=1: +/-500 dps, 65.5 LSB/dps
 * - FS_SEL=2: +/-1000 dps, 32.8 LSB/dps
 * - FS_SEL=3: +/-2000 dps, 16.4 LSB/dps
 *
 * @retval 0 Range was configured successfully.
 * @retval -EINVAL If @p dev is NULL.
 * @return Negative errno from the register write operation on failure.
 */
static int mpu6050_set_gyro_fs_dps(const struct device *dev, uint32_t dps)
{
	CHECK_NULL_PTR(dev);

	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	mpu6050_gyro_fs_t gyro_fs = mpu6050_dps_to_fs_sel(dps);

	if (gyro_fs >= MPU6050_GYRO_CONF_FS_MAX) {
		LOG_WRN("MPU6050 can't achieve %d dps, clamping to 2000dps", dps);
		gyro_fs = MPU6050_GYRO_CONF_FS_2000_DPS;
	}

	int ret = write_mask(dev, MPU6050_REG_GYRO_CONFIG, MPU6050_MASK_GYRO_CONFIG_FS_SEL,
			     (uint8_t)gyro_fs);
	if (ret != 0) {
		return ret;
	}

	data->config.gyro_fs_dps = dps;

	return 0;
}

static mpu6050_accel_fs_t mpu6050_mG_to_fs_sel(uint32_t mG)
{
	if (mG < 2000) {
		return MPU6050_ACCEL_CONF_FS_2_G;
	} else if (mG < 4000) {
		return MPU6050_ACCEL_CONF_FS_4_G;
	} else if (mG < 8000) {
		return MPU6050_ACCEL_CONF_FS_8_G;
	} else if (mG < 16000) {
		return MPU6050_ACCEL_CONF_FS_16_G;
	} else {
		LOG_WRN("MPU6050 can't achieve %d mG, clamping to 16 G", mG);
		return MPU6050_ACCEL_CONF_FS_16_G;
	}
}

/**
 * @brief Set the accelerometer full-scale range.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[in] mG Requested accelerometer range in milli-g.
 *
 * @details
 * The selected AFS_SEL value is the first range that can cover @p mG. Values
 * above 16000 mG are clamped to the 16 g range.
 *
 * - AFS_SEL=0: +/-2 g, 16384 LSB/g
 * - AFS_SEL=1: +/-4 g, 8192 LSB/g
 * - AFS_SEL=2: +/-8 g, 4096 LSB/g
 * - AFS_SEL=3: +/-16 g, 2048 LSB/g
 *
 * @retval 0 Range was configured successfully.
 * @retval -EINVAL If @p dev is NULL.
 * @return Negative errno from the register write operation on failure.
 */
int mpu6050_set_accel_fs_mG(const struct device *dev, uint32_t mG)
{
	CHECK_NULL_PTR(dev);

	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	mpu6050_accel_fs_t accel_fs = mpu6050_mG_to_fs_sel(mG);

	int ret = write_mask(dev, MPU6050_REG_ACCEL_CONFIG, MPU6050_MASK_ACCEL_CONFIG_AFS_SEL,
			     (uint8_t)accel_fs);
	if (ret != 0) {
		return ret;
	}

	data->config.accel_fs_mG = mG;

	return 0;
}

/**
 * @brief Read the temperature.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[out] tmp_mC Destination for the temperature in milli-Celsius. Must
 * not be NULL.
 *
 * @details
 * Temperature in degrees C is calculated as:
 *
 * TEMP_OUT / 340 + 36.53
 *
 * @retval 0 Temperature was read successfully.
 * @retval -EINVAL If @p dev or @p tmp_mC is NULL.
 */
int mpu6050_get_temp_mC(const struct device *dev, int16_t *tmp_mC)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(tmp_mC);

	uint8_t tmp[2];
	read_reg(dev, MPU6050_REG_TEMP_OUTH, tmp, sizeof(tmp));

	int16_t raw = sys_get_be16(&tmp[0]);
	*tmp_mC = (int16_t)((int32_t)raw * 1000 / 340);
	*tmp_mC += 36530;

	return 0;
}

static int mpu6050_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	CHECK_NULL_PTR(dev);

	if (chan != SENSOR_CHAN_ALL) {
		return -ENOTSUP;
	}

	int ret = mpu6050_get_accel(dev);
	if (ret != 0) {
		return ret;
	}

	ret = mpu6050_get_gyro(dev);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

static int mpu6050_channel_get(const struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(val);

	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	switch (chan) {
	case SENSOR_CHAN_ACCEL_XYZ:
		val = data->accel;
		break;

	case SENSOR_CHAN_GYRO_XYZ:
		val = data->gyro;
		break;

	case SENSOR_CHAN_ALL:
		val[0] = data->accel[0];
		val[1] = data->accel[1];
		val[2] = data->accel[2];
		val[3] = data->gyro[0];
		val[4] = data->gyro[1];
		val[5] = data->gyro[2];
		break;

	default:
		return -ENOTSUP;
		break;
	}
}

/**
 * @brief Initialize the MPU6050 driver.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 *
 * @retval 0 Driver was initialized successfully.
 * @retval -EINVAL If @p dev is NULL, the I2C bus is NULL, or the configured
 * I2C address is invalid.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from register access or range configuration on
 * failure.
 */
static int mpu6050_init(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;
	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	if (dev == NULL) {
		LOG_ERR("Null pointer to device.");
		return -EINVAL;
	}

	bool ready = i2c_is_ready_dt(&cfg->i2c);
	if (!ready) {
		LOG_ERR("I2C bus is not ready.");
		return -EFAULT;
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

	ret = mpu6050_set_gyro_fs_dps(dev, cfg->gyro_fs_dps);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set gyro fs (Err %d).", ret);
		return ret;
	}

	ret = mpu6050_set_accel_fs_mG(dev, cfg->accel_fs_mG);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set accel fs (Err %d).", ret);
		return ret;
	}

	LOG_INF("Initialized");
	return 0;
}

static const struct sensor_driver_api mpu6050_api = {
	.sample_fetch = mpu6050_sample_fetch,
	.channel_get = mpu6050_channel_get,
};

#define DT_DRV_COMPAT tmi_mpu6050

#define MPU6050_DEFINE(inst)                                                                       \
	static mpu6050_data_t mpu6050_data_##inst;                                                 \
                                                                                                   \
	static const mpu6050_config_t mpu6050_config_##inst = {                                    \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.alpha_div = DT_INST_PROP(inst, alpha_div),                                        \
		.accel_fs_mG = DT_INST_PROP(inst, accel_fs_mg),                                    \
		.gyro_fs_dps = DT_INST_PROP(inst, gyro_fs_dps),                                    \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, mpu6050_init, NULL, &mpu6050_data_##inst,                      \
			      &mpu6050_config_##inst, POST_KERNEL,                                 \
			      CONFIG_TMI_DRIVER_MPU6050_INIT_PRIORITY, &mpu6050_api);

DT_INST_FOREACH_STATUS_OKAY(MPU6050_DEFINE)
