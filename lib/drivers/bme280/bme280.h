
#include <zephyr/drivers/i2c.h>
#include <tmi/api/pressure.h>

#define BME280_I2C_ADDR0 0x76
#define BME280_I2C_ADDR1 0x77

typedef enum {
	BME280_GENERAL_OS_OFF = 0,
	BME280_GENERAL_OS_x1 = 1,
	BME280_GENERAL_OS_x2 = 2,
	BME280_GENERAL_OS_x4 = 3,
	BME280_GENERAL_OS_x8 = 4,
	BME280_GENERAL_OS_x16 = 5,
	BME280_GENERAL_OS_MAX = 6,
} bme280_scale_general_t;

typedef enum {
	BME280_FILTER_OS_OFF = 0,
	BME280_FILTER_OS_x2 = 1,
	BME280_FILTER_OS_x4 = 2,
	BME280_FILTER_OS_x8 = 3,
	BME280_FILTER_OS_x16 = 4,
	BME280_FILTER_OS_MAX = 5,
} bme280_scale_filter_t;

enum {
	BME280_REGISTER_DIG_T1 = 0x88,
	BME280_REGISTER_DIG_T2 = 0x8A,
	BME280_REGISTER_DIG_T3 = 0x8C,

	BME280_REGISTER_DIG_P1 = 0x8E,
	BME280_REGISTER_DIG_P2 = 0x90,
	BME280_REGISTER_DIG_P3 = 0x92,
	BME280_REGISTER_DIG_P4 = 0x94,
	BME280_REGISTER_DIG_P5 = 0x96,
	BME280_REGISTER_DIG_P6 = 0x98,
	BME280_REGISTER_DIG_P7 = 0x9A,
	BME280_REGISTER_DIG_P8 = 0x9C,
	BME280_REGISTER_DIG_P9 = 0x9E,

	BME280_REGISTER_DIG_H1 = 0xA1,
	BME280_REGISTER_DIG_H2 = 0xE1,
	BME280_REGISTER_DIG_H3 = 0xE3,
	BME280_REGISTER_DIG_H4 = 0xE4,
	BME280_REGISTER_DIG_H5 = 0xE5,
	BME280_REGISTER_DIG_H6 = 0xE7,

	BME280_REGISTER_CHIPID = 0xD0,
	BME280_REGISTER_VERSION = 0xD1,
	BME280_REGISTER_SOFTRESET = 0xE0,

	BME280_REGISTER_CAL26 = 0xE1, // R calibration stored in 0xE1-0xF0

	BME280_REGISTER_CONTROLHUMID = 0xF2,
	BME280_REGISTER_STATUS = 0XF3,
	BME280_REGISTER_CONTROL = 0xF4,
	BME280_REGISTER_CONFIG = 0xF5,
	BME280_REGISTER_PRESSUREDATA = 0xF7,
	BME280_REGISTER_TEMPDATA = 0xFA,
	BME280_REGISTER_HUMIDDATA = 0xFD
};
typedef struct {
	uint16_t dig_T1; ///< temperature compensation value
	int16_t dig_T2;  ///< temperature compensation value
	int16_t dig_T3;  ///< temperature compensation value

	uint16_t dig_P1; ///< pressure compensation value
	int16_t dig_P2;  ///< pressure compensation value
	int16_t dig_P3;  ///< pressure compensation value
	int16_t dig_P4;  ///< pressure compensation value
	int16_t dig_P5;  ///< pressure compensation value
	int16_t dig_P6;  ///< pressure compensation value
	int16_t dig_P7;  ///< pressure compensation value
	int16_t dig_P8;  ///< pressure compensation value
	int16_t dig_P9;  ///< pressure compensation value

	uint8_t dig_H1; ///< humidity compensation value
	int16_t dig_H2; ///< humidity compensation value
	uint8_t dig_H3; ///< humidity compensation value
	int16_t dig_H4; ///< humidity compensation value
	int16_t dig_H5; ///< humidity compensation value
	int8_t dig_H6;  ///< humidity compensation value
} bme280_cal_t;

typedef struct {
	struct i2c_dt_spec i2c;
	uint8_t press;
	uint8_t hum;
	uint8_t temp;
	uint8_t filter;
} bme280_config_t;

typedef struct {
	bme280_config_t config;
	uint32_t offset;
	uint64_t t_fine;
	uint32_t t_fine_adjust;
	bme280_cal_t cal;
} bme280_data_t;
