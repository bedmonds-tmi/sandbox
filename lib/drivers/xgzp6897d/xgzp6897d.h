
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

#define XGZ6897D_I2C_ADDR0 0x58

#define XGZP6897D_REGISTER_WHOAMI        0x00
#define XGZP6897D_REGISTER_CONTROL       0x01
#define XGZP6897D_REGISTER_OSR           0x02
#define XGZP6897D_REGISTER_MEASURE       0x03
#define XGZP6897D_REGISTER_PRESSURE_H    0x04
#define XGZP6897D_REGISTER_TEMPURATURE_H 0x07
#define XGZP6897D_REGISTER_CALCOE        0x20
#define XGZP6897D_REGISTER_CALCOE_2      0x21

#define XGZP6897D_TEMPURATURE_REGISTER_SIZE     2
#define SIGN_BYTE_MASK                          0x80
#define NUMBERVALUE_BYTE_MASK                   0x7F
#define XGZP6897D_TEMPERATURE_HALFWAY_THRESHOLD 32768
#define XGZP6897D_PRESSURE_HALFWAY_THRESHOLD    8388608

// XGZP6897DC005HPDPN range selected based on part model, listed in data sheet page ...
#define PMAX           500
#define PMIN           -500
#define PRESSURE_SCALE (PMAX - PMIN)

#define PA_TO_KPA 1000

#define CHECK_NULL_PTR(ptr)                                                                        \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR("%s: null pointer: " #ptr, __func__);                              \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

typedef struct {
	struct i2c_dt_spec i2c;
} xgzp6897d_config_t;

typedef struct {
	xgzp6897d_config_t config;
	uint32_t offset;
	int32_t pressure;
	uint32_t temperature;
} xgzp6897d_data_t;
