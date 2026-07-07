
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/byteorder.h>
#include <tmi/api/pressure.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(xgzp6897d);

#define XGZP6897D_REGISTER_WHOAMI 0x00

#define XGZP6897D_REGISTER_CONTROL       0x01
#define XGZP6897D_REGISTER_OSR           0x02
#define XGZP6897D_REGISTER_MEASURE       0x03
#define XGZP6897D_REGISTER_PRESSURE_H    0x04
#define XGZP6897D_REGISTER_TEMPURATURE_H 0x07
#define XGZP6897D_REGISTER_CALCOE        0x20
#define XGZP6897D_REGISTER_CALCOE_2      0x21

// XGZP6897DC005HPDPN range selected based on part # listed in data sheet
#define PMAX           500
#define PMIN           -500
#define PRESSURE_SCALE (PMAX - PMIN)

#define CHECK_NULL_PTR(ptr)                                                                        \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR("%s: null pointer: " #ptr, __func__);                              \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

// device, address, where you want to put it, lenght
static int read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	const tmi_pressure_config_t *cfg = (const tmi_pressure_config_t *)dev->config;
	return i2c_write_read(cfg->bus, cfg->addr, &reg, 1, val, len);
}

static int write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	const tmi_pressure_config_t *cfg = (const tmi_pressure_config_t *)dev->config;
	return i2c_reg_write_byte(cfg->bus, cfg->addr, reg, val);
}

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

/**
 * @brief Verify communication with the DualPortDevice.
 *
 * @param[in] dev  device instance. Must not be NULL.
 *
 * @details Reads the WHOAMI register and checks that it matches the configured
 * I2C address.
 *
 * @retval 0 The expected WHOAMI value was read.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_whoami(const struct device *dev)
{
	CHECK_NULL_PTR(dev);
	const tmi_pressure_config_t *cfg = (const tmi_pressure_config_t *)dev->config;
	uint8_t tmp;
	int ret = read_reg(dev, XGZP6897D_REGISTER_WHOAMI, &tmp, sizeof(tmp));
	if (ret != 0) {
		return ret;
	}
	printk("WHO AM I Register reads  0x%02X  \n", tmp);
	return 0;
}
/**
 * @brief updates the float variable passed to it with the temperature
 *
 * @param[in] dev  device instance.
 * @param[in] temp_C Float type.
 *
 * @details Reads 2 registers of tempurature data then uses preset values stored in a seperate
 * register for calculating the true tempurature value in degrees celcicus
 * follows this formula Final_T = (Inter_T- Byte1) / 2 ^ Shift_N + 25
 * Shift_N = Byte2/10
 * Byte1 is calculated from the 0x20
 *  Inter_T = RAW_T- 65536  or when  (RAW_T > 32768); Inter_T = RAW_T

 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_get_temp(const struct device *dev, double *temp_C)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(temp_C);
	uint8_t tmp[2] = {0, 0};
	uint8_t byte1_raw = 0;
	uint8_t byte2 = 0;
	read_reg(dev, XGZP6897D_REGISTER_TEMPURATURE_H, tmp, sizeof(tmp));
	// printk("0x%02X  0x%02X \n", tmp[0], tmp[1]);
	uint16_t raw = (uint16_t)tmp[0] << 8 | (uint16_t)tmp[1];
	read_reg(dev, XGZP6897D_REGISTER_CALCOE, &byte1_raw, sizeof(byte1_raw));
	read_reg(dev, XGZP6897D_REGISTER_CALCOE_2, &byte2, sizeof(byte2));
	// printk("byte2: 0x%02X \n", byte2);
	uint8_t Shift_N = byte2 / 10;
	int16_t interT = 0;
	if (raw > 32768) {
		interT = raw - 65536;
	} else {
		interT = raw;
	}
	// printk("inter T : %d\n ", interT);
	uint32_t power = 1 << Shift_N;
	uint32_t byte1 = 0;
	uint8_t pow = 0;
	bool sign = false;
	sign = byte1_raw & 0x80;
	pow = byte1_raw & 0x7F;
	byte1 = 1 << pow;
	// printk("byte1_raw: 0x%02X sign:%d pow: 0x%02X  byte1: %d power:%d \n", byte1_raw, sign,
	// pow, byte1, power);
	int16_t Final_T = 0;
	if (sign) {
		int16_t Numerator = interT + byte1;
		Final_T = (Numerator / (power));
	} else {
		int16_t Numerator = interT - byte1;
		Final_T = (Numerator / (power));
	}
	Final_T = Final_T + 25;
	// printk("final: %d \n", Final_T);
	*temp_C = Final_T;
	return 0;
}

/**
 * @brief updates the float variable passed to it with the pressure
 *
 * @param[in] dev  device instance.
 * @param[in] press_Pa Float type.
 *
 * @details Reads 3 registers of pressure data then uses preset PMIN and PMAX for
 * calibrating the bounds of the pressure difference read.
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_get_pressure(const struct device *dev, float *press_Pa)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(press_Pa);
	uint8_t tmp[3];
	read_reg(dev, XGZP6897D_REGISTER_PRESSURE_H, tmp, sizeof(tmp));
	// printk("0x%02X  0x%02X 0x%02X  \n", tmp[0], tmp[1], tmp[2]);
	uint32_t raw = (uint32_t)tmp[0] << 16 | (uint32_t)tmp[1] << 8 | (uint32_t)tmp[2];
	// raw &= 0xFFFFFF;
	//  printk("0x%06X \n", raw);

	float power = (1 << 21);
	float powerv2 = (1 << 24);
	float pressure_pa = 0;
	if (raw >= 8388608) {
		pressure_pa = ((float)((float)raw - powerv2) / power) * PRESSURE_SCALE;
	} else {
		pressure_pa = ((float)(raw) / power) * PRESSURE_SCALE;
	}
	*press_Pa = pressure_pa;
	return 0;
}
/**
 * @brief updates the float variable passed to it with the temperature
 *
 * @param[in] dev  device instance.
 * @param[in] RH
 * @details function parameters are required to have though this function does nothign
 * @retval 0 action prefromes nothing
 *  */
static int xgzp6897d_get_humidity(const struct device *dev, float *RH)
{
	LOG_PRINTK("Device cannot measure this parameter");
	return 0;
}

static int xgzp6897d_init_all_default(const struct device *dev)
{
	LOG_PRINTK("Device does not require initilization procedure");
	return 0;
}

const tmi_pressure_api_t xgzp6897d_api = {
	.init = xgzp6897d_init_all_default,
	.get_tempurature = xgzp6897d_get_temp,
	.get_humidity = xgzp6897d_get_humidity,
	.get_pressure = xgzp6897d_get_pressure,
	.get_whoami = xgzp6897d_whoami,
};
