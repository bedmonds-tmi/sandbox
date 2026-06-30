// #include <stdint.h>
// #include <zephyr/kernel.h>
// #include "bme280_defs.h"

// typedef struct {
// 	uint16_t dig_T1; ///< temperature compensation value
// 	int16_t dig_T2;  ///< temperature compensation value
// 	int16_t dig_T3;  ///< temperature compensation value

// 	uint16_t dig_P1; ///< pressure compensation value
// 	int16_t dig_P2;  ///< pressure compensation value
// 	int16_t dig_P3;  ///< pressure compensation value
// 	int16_t dig_P4;  ///< pressure compensation value
// 	int16_t dig_P5;  ///< pressure compensation value
// 	int16_t dig_P6;  ///< pressure compensation value
// 	int16_t dig_P7;  ///< pressure compensation value
// 	int16_t dig_P8;  ///< pressure compensation value
// 	int16_t dig_P9;  ///< pressure compensation value

// 	uint8_t dig_H1; ///< humidity compensation value
// 	int16_t dig_H2; ///< humidity compensation value
// 	uint8_t dig_H3; ///< humidity compensation value
// 	int16_t dig_H4; ///< humidity compensation value
// 	int16_t dig_H5; ///< humidity compensation value
// 	int8_t dig_H6;  ///< humidity compensation value
// } bme280_cal_t;

// typedef struct {
// 	int32_t t_fine;
// 	int32_t t_fine_adjust;
// 	bme280_cal_t cal;
// } bme280_data_t;

// typedef struct {
// 	const struct device *i2c;
// 	const uint8_t i2c_addr;
// 	bme280_scale_hum_t hum;
// 	bme280_scale_temp_t temp;
// 	bme280_scale_press_t press;
// 	bme280_scale_filter_t filter;

// } bme280_config_t;

// typedef struct {
// 	bme280_config_t config;
// 	bme280_data_t data;
// } bme280_device_t;

// int bme280_whoami(bme280_device_t *dev);

// int bme280_init_humidity(bme280_device_t *dev, bme280_scale_hum_t num);

// int bme280_init_temperature(bme280_device_t *dev, bme280_scale_temp_t num);

// int bme280_init_presssure(bme280_device_t *dev, bme280_scale_press_t num);

// int bme280_activate_filter(bme280_device_t *dev, bme280_scale_filter_t scale_of_choice);

// int bme280_get_humidity(bme280_device_t *dev, float *hum_RH);

// int bme280_get_temp(bme280_device_t *dev, float *temp_C);

// int bme280_get_pressure(bme280_device_t *dev, float *press_Pa);

// int bme280_reset(bme280_device_t *dev);

// int bme280_readCoefficients(bme280_device_t *dev);

// int bme280_activate_device(bme280_device_t *dev);
