#include <stdint.h>
#include <zephyr/kernel.h>
#include "cls16d24_defs.h"

typedef union {
	uint8_t bytes[8];
	struct {
		int16_t r;
		int16_t g;
		int16_t b;
        int16_t w;
    };
} cls16d24_rgbw_t;

typedef union {
	uint8_t bytes[2];
	struct {
		int16_t ir;
    };
} cls16d24_ir_t;

typedef struct {
    const struct device *i2c;
    const uint8_t i2c_addr;
    cls16d24_gain_fs_t gain_fs;
    cls16d24_int_time_fs_t int_time_fs;
    cls16d24_int_src_fs_t int_src_fs;
    cls16d24_diode_selt_fs_t diode_selt_fs;
} cls16d24_config_t;

typedef struct {
	cls16d24_rgbw_t rgbw_filtered;
	cls16d24_ir_t ir_filtered;
} cls16d24_data_t;

typedef struct {
	cls16d24_config_t config;
	cl16d24_data_t data;
} cls16d24_device_t;

/**
 * @brief Initalizes the CLS-16D24-44-DF8/TR8 RGBWIR sensor
 *
 */
int cls16d24_init(cls16d24_device_t *dev);
int cls16d24_check_whoami(cls16d24_device_t *dev);
int cls16d24_get_rgbw(cls16d24_device_t *dev, cls16d24_rgbw_t *val);
int cls16d24_get_ir(cls16d24_device_t *dev, cls16d24_ir_t *val);
int cls16d24_set_gain_fs(cls16d24_device_t *dev, cls16d24_gain_fs_t range);
int cls16d24_set_int_time_fs(cls16d24_device_t *dev, cls16d24_int_time_fs_t range);
int cls16d24_set_int_src_fs(cls16d24_device_t *dev, cls16d24_int_src_fs_t range);
int cls16d24_set_diode_selt_fs(cls16d24_device_t *dev, cls16d24_diode_selt_fs_t range);


/**
 * @brief Returns the value of the WHOAMI register
 *
 * This function reads reads the WHOAMI register on the sensor and prints the value within.
 *
 * @return the value within the register
 */
int cls16d24_whoami(void);

/**
 * @brief Enables the LED pin which turns on the LED
 *
 */
int cls16d24_ledenable(void);

/**
 * @brief Sets the System Control Register
 *
 * This function configures the System Control Register, a 1 is typically written to enable colour sensing.
 *
 * @param val The 8-bit integer value which controls bits 0 to 8 in the register.
 */
int cls16d24_sysmctrl(val);

/**
 * @brief Sets the INT_FLAG register
 *
 * This function configures the INT_FLAG register, a default value of 0 is typically written.
 *
 * @param val The 8-bit integer value which controls bits 0 to 8 in the register.
 */
int cls16d24_intflag(val);

/**
 * @brief Sets the WAIT_TIME register
 *
 * This function configures the WAIT_TIME register, a default value of 0 is typically written.
 *
 * @param val The 8-bit integer value which controls bits 0 to 8 in the register.
 */
int cls16d24_waittime(val);

/**
 * @brief Sets the CLS_TIME register
 *
 * This function configures the CLS_TIME register, a Value of 50 is typically written.
 *
 * @param val The 8-bit integer value which controls bits 0 to 8 in the register.
 */
int cls16d24_clstime(val);

/**
 * @brief Reads the red, green, blue and white sensed values
 *
 * This function reads the value within each register corresponding to each colour (all colours are mapped to two registers).
 * To print the values, integers red, green, blue and white must be printed.
 *
 * @param val The 8-bit integer value which controls bits 0 to 8 in the register.
 *
 * @return The sensed red, green, blue and white values
 */
int cls16d24_rgb(void);

/**
 * @brief Reads the "WHOAMI" (PROD_ID_L) register
 *
 * This function reads the PROD_ID_L register, which a hex value of 0x12 is expected.
 *
 * @return the value in the register represented as a hex value (printed)
 */
int cls16d24_whoami(void);
