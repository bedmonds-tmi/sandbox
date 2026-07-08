#ifndef TMI_API_PRESSURE_H
#define TMI_API_PRESSURE_H

#include <stdint.h>
#include <zephyr/device.h>

typedef struct {
	int (*init)(const struct device *dev);
	int (*get_humidity_RH)(const struct device *dev, uint32_t *RH);
	int (*get_pressure_Pa)(const struct device *dev, uint32_t *Pa);
	int (*get_pressure_diff_mPa)(const struct device *dev, int32_t *mPa);
	int (*get_temperature_mC)(const struct device *dev, uint32_t *mC);
} tmi_pressure_api_t;

static inline int tmi_pressure_init(const struct device *dev)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->init(dev);
}

static inline int tmi_pressure_get_humidity_RH(const struct device *dev, uint32_t *RH)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->get_humidity_RH(dev, RH);
}

static inline int tmi_pressure_get_pressure_Pa(const struct device *dev, uint32_t *Pa)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->get_pressure_Pa(dev, Pa);
}

static inline int tmi_pressure_get_pressure_diff_mPa(const struct device *dev, int32_t *mPa)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->get_pressure_diff_mPa(dev, mPa);
}

static inline int tmi_pressure_get_temp_mC(const struct device *dev, uint32_t *mC)
{
	const tmi_pressure_api_t *api = (const tmi_pressure_api_t *)dev->api;
	return api->get_temperature_mC(dev, mC);
}

#endif /* TMI_API_PRESSURE_H */
