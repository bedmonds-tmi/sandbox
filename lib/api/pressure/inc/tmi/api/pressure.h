#ifndef TMI_API_PRESSURE_H
#define TMI_API_PRESSURE_H

#include <stdint.h>
#include <zephyr/device.h>

typedef struct {
	int (*init)(const struct device *dev);
	int (*get_whoami)(const struct device *dev);
	int (*get_humidity)(const struct device *dev, float *RH);
	int (*get_pressure)(const struct device *dev, float *Pascals);
	int (*get_tempurature)(const struct device *dev, float *degC);
} tmi_pressure_api_t;

static inline int tmi_pressure_init(const struct device *dev)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->init(dev);
}
static inline int tmi_pressure_whoami(const struct device *dev)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->get_whoami(dev);
}
static inline int tmi_pressure_get_humidity(const struct device *dev, float *rh)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->get_humidity(dev, rh);
}

static inline int tmi_pressure_get_pressure(const struct device *dev, float *pa)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->get_pressure(dev, pa);
}

static inline int tmi_pressure_get_temp_mC(const struct device *dev, float *mC)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->get_tempurature(dev, mC);
}

#endif /* TMI_API_PRESSURE_H */
