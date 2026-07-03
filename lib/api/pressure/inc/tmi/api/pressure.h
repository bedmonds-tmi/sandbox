#ifndef TMI_API_PRESSURE_H
#define TMI_API_PRESSURE_H

#include <stdint.h>
#include <zephyr/device.h>

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
	const struct device *bus;
	uint8_t addr;
	uint8_t filter;
	uint8_t hum;
	uint8_t temp;
	uint8_t press;

} tmi_pressure_config_t;

typedef struct {
	int32_t t_fine;
	int32_t t_fine_adjust;
	bme280_cal_t cal;
} tmi_pressure_data_t;

typedef struct tmi_pressure_s tmi_pressure_t;

typedef struct {
	int (*init)(tmi_pressure_t *dev);
	int (*get_whoami)(tmi_pressure_t *dev);
	int (*get_humidity)(tmi_pressure_t *dev, float *RH);
	int (*get_pressure)(tmi_pressure_t *dev, float *Pascals);
	int (*get_tempurature)(tmi_pressure_t *dev, double *degC);
} tmi_pressure_api_t;

struct tmi_pressure_s {
	tmi_pressure_config_t config;
	tmi_pressure_data_t data;
	const tmi_pressure_api_t *api;
};

static inline int tmi_pressure_init(tmi_pressure_t *dev)
{
	return dev->api->init(dev);
}
static inline int tmi_pressure_whoami(tmi_pressure_t *dev)
{
	return dev->api->get_whoami(dev);
}
static inline int tmi_pressure_get_humidity(tmi_pressure_t *dev, float *rh)
{
	return dev->api->get_humidity(dev, rh);
}

static inline int tmi_pressure_get_pressure(tmi_pressure_t *dev, float *pa)
{
	return dev->api->get_pressure(dev, pa);
}

static inline int tmi_pressure_get_temp_mC(tmi_pressure_t *dev, double *mC)
{
	return dev->api->get_tempurature(dev, mC);
}

#endif /* TMI_API_PRESSURE_H */
