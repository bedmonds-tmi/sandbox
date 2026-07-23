/**
 * @file cls16d24_defs.h
 * @brief
 * @details
 */

#include <zephyr/drivers/i2c.h>
#include <tmi/api/light.h>

#define CLS16D24_I2C_ADDR0 0x38
#define CLS16D24_I2C_ADDR1 0x39

/**
 * @brief Full scale gain conf selections
 */
typedef enum {
	CLS16D24_GAIN_CONF_FS_X1 = 0,
	CLS16D24_GAIN_CONF_FS_X4 = 1,
	CLS16D24_GAIN_CONF_FS_X8 = 2,
	CLS16D24_GAIN_CONF_FS_X32 = 3,
	CLS16D24_GAIN_CONF_FS_X96 = 4,
	CLS16D24_GAIN_CONF_FS_MAX = 5,
} cls16d24_gain_fs_t;

/**
 * @brief Full scale int time conf selections
 */
typedef enum {
	CLS16D24_INT_TIME_CONF_FS_T = 0,
	CLS16D24_INT_TIME_CONF_FS_4T = 1,
	CLS16D24_INT_TIME_CONF_FS_16T = 2,
	CLS16D24_INT_TIME_CONF_FS_64T = 3,
	CLS16D24_INT_TIME_CONF_FS_MAX = 4,
} cls16d24_int_time_fs_t;

/**
 * @brief Full scale interupt source conf selections
 */
typedef enum {
	CLS16D24_INT_SRC_CONF_FS_RCH_DATA = 0,
	CLS16D24_INT_SRC_CONF_FS_GCH_DATA = 1,
	CLS16D24_INT_SRC_CONF_FS_BCG_DATA = 2,
	CLS16D24_INT_SRC_CONF_FS_WCH_DATA = 3,
	CLS16D24_INT_SRC_CONF_FS_IRCH_DATA = 4,
	CLS16D24_INT_SRC_CONF_FS_MAX = 5,
} cls16d24_int_src_fs_t;

/**
 * @brief Full scale sensor area conf selections
 */
typedef enum {
	CLS16D24_DIODE_SELT_CONF_FS_X1 = 0,
	CLS16D24_DIODE_SELT_CONF_FS_X2 = 1,
	CLS16D24_DIODE_SELT_CONF_FS_MAX = 2,
} cls16d24_diode_selt_fs_t;

typedef struct {
	struct i2c_dt_spec i2c;
	uint16_t gain_fs_x;
	uint16_t int_time_fs_t;
	uint16_t int_src_fs_dt;
	uint16_t diode_selt_fs_x;
} cls16d24_config_t;

typedef struct {
	cls16d24_config_t config;
	tmi_light_rgbw_t rgbw;
	tmi_light_ir_t ir;
} cls16d24_data_t;
