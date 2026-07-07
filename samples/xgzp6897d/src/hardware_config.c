#include <tmi/api/pressure.h>

extern const tmi_pressure_api_t xgzp6897d_api;
#define PRESSURE1 xgzp6897d_api

tmi_pressure_config_t p1_config = {
	.bus = DEVICE_DT_GET(DT_ALIAS(i2c0)),
	.addr = 0x58,
	.filter = 0,
};

struct device p1 = {
	.config = &p1_config,
	.api = &PRESSURE1,
};
