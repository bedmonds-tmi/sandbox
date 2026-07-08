
// #include "bme280.h"
#include <tmi/api/pressure.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/byteorder.h>
#include "bme280.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bme280);

#define BME280_REGISTER_DIG_T1       0x88
#define BME280_REGISTER_DIG_T2       0x8A
#define BME280_REGISTER_DIG_T3       0x8C
#define BME280_REGISTER_WHOAMI       0xD0
#define BME280_REGISTER_VERSION      0xD1
#define BME280_REGISTER_SOFTRESET    0xE0
#define BME280_REGISTER_CAL26        0xE1
#define BME280_REGISTER_CONTROLHUMID 0xF2
#define BME280_REGISTER_STATUS       0XF3
#define BME280_REGISTER_CONTROL      0xF4
#define BME280_REGISTER_CONFIG       0xF5

#define BME280_REGISTER_PRESSUREDATA_H 0xF7
#define BME280_REGISTER_TEMPDATA_H     0xFA
#define BME280_REGISTER_HUMIDDATA_H    0xFD

// standard register values
#define BME280_VALUE_RESET 0xB6

/**
 * @brief reads a set value from a register of choice
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] reg  register you want to change
 * @param[in] val  a variable that will contain the contents read
 * @param[in] len  length of the register being read
 *
 * @retval 0        On success.
 * @retval -errno   Standard negative error code on failure.
 */
static int read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	const bme280_config_t *cfg = (const bme280_config_t *)dev->config;
	return i2c_write_read_dt(&cfg->i2c, &reg, 1, val, len);
}
/**
 * @brief writes a set value to a register of choice
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] reg  register you want to change
 * @param[in] val value you would like to replace the range of bits with
 *
 * @retval 0        On success.
 * @retval -errno   Standard negative error code on failure.
 */
static int write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	const bme280_config_t *cfg = (const bme280_config_t *)dev->config;
	return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}
/**
 * @brief writes a set value to a set # of bits
 *
 * @param[in] dev  device instance. Must not be NULL.
 * @param[in] reg  register you want to change
 * @param[in] mask bit mask of 1's ranging section of desired change
 * @param[in] val value you would like to replace the range of bits with
 *
 * @retval 0        On success.
 * @retval -errno   Standard negative error code on failure.
 */
static int write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t tmp;

	int ret = read_reg(dev, reg, &tmp, sizeof(tmp));
	if (ret != 0) {
		printk("Error: %d", ret);
		return ret;
	}
	tmp &= ~mask;                 // Clear the target bit
	tmp |= FIELD_PREP(mask, val); // Set the target bit based on val
	return write_reg(dev, reg, tmp);
}

/*!
 * @brief  Reads the factory-set coefficients
 * @details Reads the factory set coefficients of the device and assignes them to variables in the
 * data that can be fetched later for calculations
 * @param[in] dev  device instance. Must not be NULL.
 *
 * @retval 0 The expected WHOAMI value was read.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_readCoefficients(const struct device *dev)
{
	bme280_data_t *data = (bme280_data_t *)dev->data;
	uint8_t val[2];

	read_reg(dev, BME280_REGISTER_DIG_T1, val, sizeof(val));
	data->cal.dig_T1 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_T2, val, sizeof(val));
	data->cal.dig_T2 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_T3, val, sizeof(val));
	data->cal.dig_T3 = sys_get_le16(val);

	read_reg(dev, BME280_REGISTER_DIG_P1, val, sizeof(val));
	data->cal.dig_P1 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_P2, val, sizeof(val));
	data->cal.dig_P2 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_P3, val, sizeof(val));
	data->cal.dig_P3 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_P4, val, sizeof(val));
	data->cal.dig_P4 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_P5, val, sizeof(val));
	data->cal.dig_P5 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_P6, val, sizeof(val));
	data->cal.dig_P6 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_P7, val, sizeof(val));
	data->cal.dig_P7 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_P8, val, sizeof(val));
	data->cal.dig_P8 = sys_get_le16(val);
	read_reg(dev, BME280_REGISTER_DIG_P9, val, sizeof(val));
	data->cal.dig_P9 = sys_get_le16(val);

	uint8_t val1, val2;

	read_reg(dev, BME280_REGISTER_DIG_H1, &val1, sizeof(val1));
	data->cal.dig_H1 = val1;

	read_reg(dev, BME280_REGISTER_DIG_H2, val, sizeof(val));
	data->cal.dig_H2 = sys_get_le16(val);

	read_reg(dev, BME280_REGISTER_DIG_H3, &val1, sizeof(val1));
	data->cal.dig_H3 = val1;

	read_reg(dev, BME280_REGISTER_DIG_H4, &val1, sizeof(val));
	read_reg(dev, BME280_REGISTER_DIG_H4 + 1, &val2, sizeof(val2));
	data->cal.dig_H4 = (val1 << 4) | (val2 & 0xF);

	read_reg(dev, BME280_REGISTER_DIG_H5 + 1, &val1, sizeof(val1));
	read_reg(dev, BME280_REGISTER_DIG_H4, &val2, sizeof(val2));

	data->cal.dig_H5 = ((val1) << 4) | (val2 >> 4);

	read_reg(dev, BME280_REGISTER_DIG_H6, &val1, sizeof(val1));
	data->cal.dig_H6 = val1;

	return 0;
}

/**
 * @brief Verify communication with the BME280.
 *
 * @details Reads the WHOAMI register and checks that it matches the configured
 * I2C address.
 * @param[in] dev  device instance. Must not be NULL.
 *
 * @retval 0 The expected WHOAMI value was read.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_whoami(const struct device *dev)
{
	if (dev == NULL) {
		return -EINVAL;
	}
	uint8_t tmp;
	int ret = read_reg(dev, BME280_REGISTER_WHOAMI, &tmp, sizeof(tmp));
	if (ret != 0) {
		return ret;
	}
	LOG_INF("WHO AM I Register reads  0x%02X ", tmp);
	return 0;
}

/**
 * @brief toggles the reset register.
 *
 * @details Writing BME280_VALUE_RESET to the reset will cause the device to undergo a procedure
 * where it sets all other registers to thier default values
 * @param[in] dev  device instance. Must not be NULL.
 *
 * @retval 0 The expected WHOAMI value was read.
 * @retval -EINVAL If @p dev is NULL.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_reset(const struct device *dev)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	int ret = write_reg(dev, BME280_REGISTER_SOFTRESET, BME280_VALUE_RESET);
	if (ret != 0) {
		return ret;
	}
	printk("Reset ....");

	return 0;
}

/**
 * @brief Verify communication with the DualPortDevice.
 *
 * @details Reads the WHOAMI register and checks that it matches the configured
 * I2C address.
 * @param[in] dev  device instance. Must not be NULL.
 *
 * @retval 0 The expected WHOAMI value was read.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_activate_device(const struct device *dev)
{
	uint8_t val;
	int ret = read_reg(dev, BME280_REGISTER_CONTROL, &val, sizeof(val));
	if (ret != 0) {
		return ret;
	}
	printk("Control register parameters :  0x%02X ", val);

	ret = write_reg(dev, BME280_REGISTER_CONTROL, 0x03);
	if (ret != 0) {
		return ret;
	}
}

/**
 * @brief Verify communication with the DualPortDevice.
 * @details takes a number and returns an enum .
 * @param[in] factor  scale factor .
 *
 * @return one of the ENUM listed in teh bme280.h
 */
static bme280_scale_general_t bme280_num_to_scale_sel(uint32_t factor)
{

	if (factor < 2) {
		return BME280_GENERAL_OS_x2;
	} else if (factor < 4) {
		return BME280_GENERAL_OS_x4;
	} else if (factor < 8) {
		return BME280_GENERAL_OS_x8;
	} else if (factor < 16) {
		return BME280_GENERAL_OS_x16;
	} else {
		printk("bme280 can't achieve %d factor, clamping to 16 G", factor);
		return BME280_GENERAL_OS_x16;
	}
}

/**
 * @brief Set the Humidity oversampling range.
 * @details
 * The selected OS_MULT will change how many values are averaged before producing an output
 * Options are
 * 1 = x1
 * 2 = x2
 * 3 = x4
 * 4 = x8
 * 5 = x16
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[in] num Requested humidity oversampling
 *
 * @retval 0 Oversampling was configured successfully.
 * @retval -EINVAL If @p dev is NULL.
 * @return Negative errno from the register write operation on failure.
 */
static int bme280_init_humidity(const struct device *dev, uint8_t num)
{

	if (num >= BME280_GENERAL_OS_x16 && num < 0) {
		printk("Invalid gyro fs range.");
		return -ERANGE;
	}
	bme280_scale_general_t humidity_s = bme280_num_to_scale_sel(num);

	int ret = write_reg(dev, BME280_REGISTER_CONTROLHUMID, (uint8_t)humidity_s);
	if (ret != 0) {
		return ret;
	}
	bme280_data_t *data = (bme280_data_t *)dev->config;
	data->config.hum = num;
	uint8_t x;
	read_reg(dev, BME280_REGISTER_CONTROLHUMID, &x, sizeof(x));
	printk("reading of control register 0x%02X", x);

	return 0;
}

/**
 * @brief Set the temperature oversampling range.
 * @details
 * The selected OS_MULT will change how many values are averaged before producing an output
 * Options are
 * 1 = x1
 * 2 = x2
 * 3 = x4
 * 4 = x8
 * 5 = x16
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[in] num Requested temperature oversampling
 *
 * @retval 0 Oversampling was configured successfully.
 * @retval -EINVAL If @p dev is NULL.
 * @return Negative errno from the register write operation on failure.
 */
static int bme280_init_temperature(const struct device *dev, uint8_t num)
{

	if (num >= BME280_GENERAL_OS_x16 && num < 0) {
		printk("Invalid gyro fs range.");
		return -ERANGE;
	}
	bme280_scale_general_t temperature_s = bme280_num_to_scale_sel(num);

	int ret = write_mask(dev, BME280_REGISTER_CONTROL, 0xE0, (uint8_t)temperature_s);
	if (ret != 0) {
		return ret;
	}
	bme280_config_t *cfg = (bme280_config_t *)dev->config;
	cfg->temp = num;
	return 0;
}

/**
 * @brief Set the pressure oversampling range.
 *
 * @details
 * The selected OS_MULT will change how many values are averaged before producing an output
 * Options are
 * 1 = x1
 * 2 = x2
 * 3 = x4
 * 4 = x8
 * 5 = x16
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[in] num Requested pressure oversampling
 *
 * @retval 0 Oversampling was configured successfully.
 * @retval -EINVAL If @p dev is NULL.
 * @return Negative errno from the register write operation on failure.
 */
static int bme280_init_pressure(const struct device *dev, uint8_t num)
{
	if (num >= BME280_GENERAL_OS_x16 && num < 0) {
		printk("Invalid gyro fs range.");
		return -ERANGE;
	}
	bme280_scale_general_t pressure_s = bme280_num_to_scale_sel(num);

	int ret = write_mask(dev, BME280_REGISTER_CONTROL, 0x1C, (uint8_t)pressure_s);
	if (ret != 0) {
		return ret;
	}
	bme280_config_t *cfg = (bme280_config_t *)dev->config;
	cfg->press = num;
	return 0;
}
/**
 * @brief updates the float variable passed to it with the temperature
 * @details Reads 2 registers of tempurature data then uses preset values stored in a seperate
 * register for calculating the true tempurature value in degrees celcicus
 * follows this formula
 *
 * @param[in] dev  device instance.
 * @param[in] temp_C Float type.
 *
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_get_temp(const struct device *dev, float *degC)
{
	bme280_data_t *data = (bme280_data_t *)dev->data;
	uint8_t tmp[3];
	uint32_t combined_data;
	read_reg(dev, BME280_REGISTER_TEMPDATA_H, tmp, sizeof(tmp));
	uint16_t upper16 = sys_get_be16(&tmp[0]);
	combined_data = (uint32_t)upper16 << 4;
	combined_data |= (uint32_t)tmp[2] && 0xF0;
	int32_t var1, var2;
	var1 = ((combined_data / 8) - ((int32_t)data->cal.dig_T1 * 2));
	var1 = (var1 * ((int32_t)data->cal.dig_T2)) / 2048;
	var2 = ((combined_data / 16) - ((int32_t)data->cal.dig_T1));
	var2 = (((var2 * var2) / 4096) * ((int32_t)data->cal.dig_T3)) / 16384;
	data->t_fine = var1 + var2 + data->t_fine_adjust;
	int32_t T = (data->t_fine * 5 + 128) / 256;
	*degC = (float)T / 100;
	return 0;
}

/**
 * @brief updates the float variable passed to it with the Humidity
 *
 * @details Reads 2 registers of tempurature data then uses preset values stored in a seperate
 * register for calculating the true tempurature value in degrees celcicus
 * follows this formula
 * @param[in] dev  device instance.
 * @param[in] hum_RH relative humidity float type.
 *
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_get_humidity(const struct device *dev, float *hum_RH)
{

	uint8_t tmp[2];
	read_reg(dev, BME280_REGISTER_HUMIDDATA_H, tmp, sizeof(tmp));
	uint16_t raw = sys_get_be16(&tmp[0]);
	bme280_data_t *data = (bme280_data_t *)dev->data;
	int32_t var1, var2, var3, var4, var5;
	float temp;
	int ret = bme280_get_temp(dev, &temp);
	if (ret != 0) {
		return ret;
	} // must be done first to get t_fine
	var1 = data->t_fine - ((int32_t)76800);
	var2 = (int32_t)(raw * 16384);
	var3 = (int32_t)(((int32_t)data->cal.dig_H4) * 1048576);
	var4 = ((int32_t)data->cal.dig_H4) * var1;
	var5 = (((var2 - var3) - var4) + (int32_t)16384) / 32768;
	var2 = (var1 * ((int32_t)data->cal.dig_H6)) / 1024;
	var3 = (var1 * ((int32_t)data->cal.dig_H3)) / 2048;
	var4 = ((var2 * (var3 + (int32_t)32768)) / 1024) + (int32_t)2097152;
	var2 = ((var4 * ((int32_t)data->cal.dig_H2)) + 8192) / 16384;
	var3 = var5 * var2;
	var4 = ((var3 / 32768) * (var3 / 32768)) / 128;
	var5 = var3 - ((var4 * ((int32_t)data->cal.dig_H1)) / 16);
	var5 = (var5 < 0 ? 0 : var5);
	var5 = (var5 > 419430400 ? 419430400 : var5);
	uint32_t H = (uint32_t)(var5 / 4096);
	*hum_RH = (float)H / 1024.0;
	return 0;
}
/**
 * @brief updates the float variable passed to it with the pressure
 * @details Reads 2 registers of pressure data then uses preset values stored in a seperate
 * register for calculating the true pressure value in degrees celcicus
 * follows this formula
 *
 *
 * @param[in] dev  device instance.
 * @param[in] press_Pa relative pressure float type.
 *
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_get_pressure(const struct device *dev, float *press_Pa)
{
	bme280_data_t *data = (bme280_data_t *)dev->data;
	uint8_t tmp[3];
	uint32_t combined_data;
	read_reg(dev, BME280_REGISTER_TEMPDATA_H, tmp, sizeof(tmp));
	uint16_t upper16 = sys_get_be16(&tmp[0]);
	combined_data = (uint32_t)upper16 << 4;
	combined_data |= (uint32_t)tmp[2] && 0xF0;
	int64_t var1, var2, var3, var4;
	float temp;
	int ret = bme280_get_temp(dev, &temp);
	if (ret != 0) {
		return ret;
	}
	var1 = ((int64_t)data->t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)data->cal.dig_P6;
	var2 = var2 + ((var1 * (int64_t)data->cal.dig_P5) * 131072);
	var2 = var2 + (((int64_t)data->cal.dig_P4) * 34359738368);
	var1 = ((var1 * var1 * (int64_t)data->cal.dig_P3) / 256) +
	       ((var1 * ((int64_t)data->cal.dig_P2) * 4096));
	var3 = ((int64_t)1) * 140737488355328;
	var1 = (var3 + var1) * ((int64_t)data->cal.dig_P1) / 8589934592;
	if (var1 == 0) {
		return 0; // avoid exception caused by division by zero
	}
	var4 = 1048576 - combined_data;
	var4 = (((var4 * 2147483648) - var2) * 3125) / var1;
	var1 = (((int64_t)data->cal.dig_P9) * (var4 / 8192) * (var4 / 8192)) / 33554432;
	var2 = (((int64_t)data->cal.dig_P8) * var4) / 524288;
	var4 = ((var4 + var1 + var2) / 256) + (((int64_t)data->cal.dig_P7) * 16);
	*press_Pa = var4 / 256.0;
	return 0;
}
/**
 * @brief sets the built in IIR filter for the pressure values
 *
 * @details it reduces the bandwidth of the temperature and pressure output signals and
 * increases the resolution fo the pressure and hte temperature output data to 20 bit.
 * The output ofa next measuremtn step is filtered usedin the following formula:
 *
 * @param[in] dev  device instance.
 * @param[in] scale_of_choice an int number between 0- 5 that will change the number of samples
 *
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_activate_filter(const struct device *dev, uint8_t scale_of_choice)
{
	bme280_config_t *cfg = (bme280_config_t *)dev->config;
	if (scale_of_choice >= BME280_FILTER_OS_MAX && scale_of_choice < 0) {
		printk("Invalid scale range.");
		return -ERANGE;
	}
	int ret = write_mask(dev, BME280_REGISTER_CONFIG, 0x1C, (uint8_t)scale_of_choice);
	if (ret != 0) {
		return ret;
	}
	cfg->filter = 2;
	return 0;
}
/**
 * @brief sets the built in IIR filter for the pressure values
 *
 * @details initializes the device with basic parameters outlined in the device tree
 * using all the initialzation functions and the activate function
 * previously defined
 *
 * @param[in] dev  device instance.
 *
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int bme280_init(const struct device *dev)
{
	int ret = bme280_readCoefficients(dev);
	if (ret != 0) {
		printk("Init failed: %d  \n", ret);
	}
	bme280_config_t *cfg = (bme280_config_t *)dev->config;
	ret = bme280_whoami(dev);
	if (ret != 0) {
		printk("Init failed: %d  \n", ret);
	}
	ret = bme280_init_temperature(dev, cfg->temp);
	if (ret != 0) {
		printk("Init failed: %d  \n", ret);
	}
	ret = bme280_init_humidity(dev, cfg->hum);
	if (ret != 0) {
		printk("Init failed: %d  \n", ret);
	}
	ret = bme280_init_pressure(dev, cfg->press);
	if (ret != 0) {
		printk("Init failed: %d  \n", ret);
	}

	ret = bme280_activate_filter(dev, cfg->filter);
	if (ret != 0) {
		printk("Init failed: %d  \n", ret);
	}

	ret = bme280_activate_device(dev);
	if (ret != 0) {
		printk("Init failed: %d  \n", ret);
	}
	return 0;
}

const tmi_pressure_api_t bme280_api = {
	.init = bme280_init,
	.get_tempurature = bme280_get_temp,
	.get_humidity = bme280_get_humidity,
	.get_pressure = bme280_get_pressure,
	.get_whoami = bme280_whoami,
};

#define DT_DRV_COMPAT tmi_bme280

#define BME280_DEFINE(inst)                                                                        \
	static bme280_data_t bme280_data_##inst;                                                   \
                                                                                                   \
	static const bme280_config_t bme280_config_##inst = {                                      \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.press = DT_INST_PROP(inst, press),                                                \
		.hum = DT_INST_PROP(inst, hum),                                                    \
		.temp = DT_INST_PROP(inst, temp),                                                  \
		.filter = DT_INST_PROP(inst, filter),                                              \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, bme280_init, NULL, &bme280_data_##inst, &bme280_config_##inst, \
			      POST_KERNEL, CONFIG_TMI_DRIVER_BME280_INIT_PRIORITY, &bme280_api);

DT_INST_FOREACH_STATUS_OKAY(BME280_DEFINE)
