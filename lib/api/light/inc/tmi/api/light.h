#ifndef TMI_API_LIGHT_H
#define TMI_API_LIGHT_H

#include <stdint.h>
#include <zephyr/device.h>

typedef union {
	uint8_t bytes[8];
	struct {
		int16_t r;
		int16_t g;
		int16_t b;
		int16_t w;
	};
} tmi_light_rgbw_t;

typedef struct {
	int (*init)(const struct device *dev);
	int (*get_rgbw)(const struct device *dev, tmi_light_rgbw_t *raw);
	int (*get_ir)(const struct device *dev, tmi_light_ir_t *raw);
	int (*set_en_ir_fs_ir)(const struct device *dev, uint32_t ir);
	int (*set_gain_fs_x)(const struct device *dev, int32_t x);
	int (*set_int_time_fs_time)(const struct device *dev, int32_t time);
	int (*set_int_src_fs_dt)(const struct device *dev, int32_t dt);
	int (*set_diode_selt_fs_x)(const struct device *dev, int32_t x);
} tmi_light_api_t;

static inline int tmi_light_init(const struct device *dev)
{
	const tmi_light_api_t *api = (const tmi_light_api_t *)dev->api;
	return api->init(dev);
}

static inline int tmi_light_get_rgbw(const struct device *dev, tmi_light_rgbw_t *raw)
{
	const tmi_light_api_t *api = (const tmi_light_api_t *)dev->api;
	return api->get_rgbw(dev, raw);
}

static inline int tmi_light_get_ir(const struct device *dev, tmi_light_ir_t *raw)
{
	const tmi_light_api_t *api = (const tmi_light_api_t *)dev->api;
	return api->get_ir(dev, raw);
}

static inline int tmi_light_set_en_ir_fs_ir(const struct device *dev, uint32_t ir)
{
	const tmi_light_api_t *api = (const tmi_light_api_t *)dev->api;
	return api->set_en_ir_fs_ir(dev, ir);
}

static inline int tmi_light_set_gain_fs_x(const struct device *dev, int32_t x)
{
	const tmi_light_api_t *api = (const tmi_light_api_t *)dev->api;
	return api->set_gain_fs_x(dev, x);
}

static inline int tmi_light_set_int_time_fs_time(const struct device *dev, int32_t time)
{
	const tmi_light_api_t *api = (const tmi_light_api_t *)dev->api;
	return api->set_int_time_fs_time(dev, time);
}

static inline int tmi_light_set_int_src_fs_dt(const struct device *dev, int32_t dt)
{
	const tmi_light_api_t *api = (const tmi_light_api_t *)dev->api;
	return api->set_int_src_fs_dt(dev, dt);
}

static inline int tmi_light_set_diode_selt_fs_x(const struct device *dev, int32_t x))
{
	const tmi_light_api_t *api = (const tmi_light_api_t *)dev->api;
	return api->set_diode_selt_fs_x(dev, x);
}

#endif /* TMI_API_LIGHT_H */
