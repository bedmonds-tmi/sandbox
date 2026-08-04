#include "tcs3400.h"

LOG_MODULE_DECLARE(tcs3400);

/**
 * @brief Map a numerical scale value to the closest supported device gain.
 *
 * @details
 * This function takes a desired scale factor and selects the
 * hardware gain enum that fits it. If the requested scale exceeds the
 * hardware maximum (64x), it issues a warning and defaults to the max setting.
 *
 * @param[in] scale The desired multiplication scale factor.
 *
 * @return The corresponding tcs3400_color_gain_t hardware enum value.
 */
tcs3400_color_gain_t tcs3400_scale_gain(uint32_t scale)
{
	if (scale <= 1) {
		return TCS3400_CONF_FS_1x_DPS;
	} else if (scale <= 4) {
		return TCS3400_CONF_FS_4x_DPS;
	} else if (scale <= 16) {
		return TCS3400_CONF_FS_16x_DPS;
	} else if (scale <= 64) {
		return TCS3400_CONF_FS_64x_DPS;
	} else {
		LOG_WRN("TCS3400 can't achieve %d scale, clamping to 64x", scale);
		return TCS3400_CONF_FS_64x_DPS;
	}
}

/**
 * @brief Map a raw cycle count to the closest supported integration cycle step.
 *
 * @details
 * This function picks the matching hardware integration cycle enum
 * based on the requested count. If the input exceeds the 64-cycle threshold,
 * it issues a warning and caps the output at the maximum 256-cycle setting.
 *
 * @param[in] cycles The requested number of integration cycles.
 *
 * @return The corresponding tcs3400_integration_cycles_t hardware enum value.
 */
tcs3400_integration_cycles_t tcs3400_scale_integration(uint32_t cycles)
{
	if (cycles <= 1) {
		return TCS3400_CONF_CYCLES_1;
	} else if (cycles <= 10) {
		return TCS3400_CONF_CYCLES_10;
	} else if (cycles <= 37) {
		return TCS3400_CONF_CYCLES_37;
	} else if (cycles <= 64) {
		return TCS3400_CONF_CYCLES_64;
	} else {
		LOG_WRN("TCS3400 can't achieve %d cycles, clamping to 256", cycles);
		return TCS3400_CONF_CYCLES_256_MAX;
	}
}
