#include <tmi/api/pressure.h>

extern const tmi_pressure_api_t bme280_api;
#define PRESSURE0 bme280_api

tmi_pressure_config_t p0_config = {
	.bus = DEVICE_DT_GET(DT_ALIAS(i2c0)),
	.addr = 0x58,
	.filter = 0,
};

struct device p0 = {
	.config = &p0_config,
	.api = &PRESSURE0,
};
