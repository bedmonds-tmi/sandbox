
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
static int read_reg(const tmi_pressure_t *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	return i2c_write_read(dev->config.bus, dev->config.addr, &reg, 1, val, len);
}

static int write_reg(const tmi_pressure_t *dev, uint8_t reg, uint8_t val)
{
	return i2c_reg_write_byte(dev->config.bus, dev->config.addr, reg, val);
}

static int write_mask(tmi_pressure_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
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
static int xgzp6897d_whoami(tmi_pressure_t *dev)
{
	CHECK_NULL_PTR(dev);
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
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_get_temp(tmi_pressure_t *dev, float *temp_C)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(temp_C);
	uint8_t tmp[2];
	int8_t byte1, byte2;
	read_reg(dev, XGZP6897D_REGISTER_TEMPURATURE_H, tmp, sizeof(tmp));
	// printk("0x%02X  0x%02X \n", tmp[0], tmp[1]);
	uint16_t raw = (uint16_t)tmp[0] << 8 | (uint16_t)tmp[1];
	read_reg(dev, XGZP6897D_REGISTER_CALCOE, &byte1, sizeof(byte1));
	read_reg(dev, XGZP6897D_REGISTER_CALCOE_2, &byte2, sizeof(byte2));

	printk("0x%02X \n", byte1);

	int Shift_N = (float)byte2 / 10;
	float interT = 0.0f;
	if (raw > 32768) {
		interT = raw - 65536;
	} else {
		interT = raw;
	}
	int power = 2 ^ Shift_N;
	float Final_T = ((float)(interT - byte1) / (float)(power)) + 25;
	*temp_C = Final_T;
	return 0;
}
/**
 * @brief updates the float variable passed to it with the pressure
 *
 * @param[in] dev  device instance.
 * @param[in] press_Pa Float type.
 *
 * @details Reads 3 registers of pressure data then uses preset PMIN and PMAX for calibrating the
 * bounds of the pressure difference read.
 * @retval 0 The expected if completed Nominally.
 * @return Negative errno from the register read operation on failure.
 */
static int xgzp6897d_get_pressure(tmi_pressure_t *dev, float *press_Pa)
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

	if (raw >= 8388608) {
		*press_Pa = ((float)((float)raw - powerv2) / power) * PRESSURE_SCALE;
	} else {
		*press_Pa = ((float)(raw) / power) * PRESSURE_SCALE;
	}
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
static int xgzp6897d_get_humidity(tmi_pressure_t *dev, float *RH)
{
	LOG_PRINTK("Device cannot measure this parameter");
	return 0;
}

static int xgzp6897d_init_all_default(tmi_pressure_t *dev)
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
