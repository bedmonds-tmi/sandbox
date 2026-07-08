
#include <zephyr/drivers/i2c.h>
#include <tmi/api/pressure.h>
#define XGZ6897D_I2C_ADDR0 0x58

typedef struct {
	struct i2c_dt_spec i2c;
} xgzp6897d_config_t;

typedef struct {
	xgzp6897d_config_t config;
	uint32_t offset;
} xgzp6897d_data_t;
