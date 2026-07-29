/**
 * @file mpu6050_utils.c
 * @brief Conversions between FS_SEL register values and physical units for
 * the MPU6050 accelerometer and gyroscope.
 *
 * @details Holds the per-axis full-scale range tables (the only hardcoded
 * hardware constants) and derives everything else from them: FS_SEL to
 * range, range to nearest achievable FS_SEL, and FS_SEL to ADC sensitivity
 * (LSB per unit). Also provides the generic physical-unit conversions
 * (mG/ms^2, dps/rad-s) needed to translate between the Zephyr sensor API's
 * units and the ones this driver's registers use.
 */

#include "mpu6050.h"

LOG_MODULE_DECLARE(mpu6050);

/** The 16-bit signed ADC output spans +/-32768 across the full-scale range. */
#define MPU6050_ADC_FS_COUNTS 32768.0

static const uint32_t mpu6050_accel_fs_range_mG[MPU6050_ACCEL_CONF_FS_MAX] = {
	[MPU6050_ACCEL_CONF_FS_2_G] = 2000,
	[MPU6050_ACCEL_CONF_FS_4_G] = 4000,
	[MPU6050_ACCEL_CONF_FS_8_G] = 8000,
	[MPU6050_ACCEL_CONF_FS_16_G] = 16000,
};

/**
 * @brief Actual accelerometer full-scale range, in mG, for a given FS_SEL.
 */
uint32_t mpu6050_fs_to_mG(mpu6050_accel_fs_t fs)
{
	return mpu6050_accel_fs_range_mG[fs];
}

/**
 * @brief Accelerometer sensitivity, in LSB per g, for a given full-scale range.
 */
double mpu6050_accel_fs_to_sensitivity(mpu6050_accel_fs_t fs)
{
	double range_g = (double)mpu6050_accel_fs_range_mG[fs] / 1000.0;

	return MPU6050_ADC_FS_COUNTS / range_g;
}

mpu6050_accel_fs_t mpu6050_mG_to_fs(uint32_t mG)
{
	for (mpu6050_accel_fs_t fs = MPU6050_ACCEL_CONF_FS_2_G; fs < MPU6050_ACCEL_CONF_FS_16_G;
	     fs++) {
		if (mG < mpu6050_accel_fs_range_mG[fs]) {
			return fs;
		}
	}

	if (mG >= mpu6050_accel_fs_range_mG[MPU6050_ACCEL_CONF_FS_16_G]) {
		LOG_WRN("MPU6050 can't achieve %d mG, clamping to 16 G", mG);
	}

	return MPU6050_ACCEL_CONF_FS_16_G;
}

static const uint32_t mpu6050_gyro_fs_range_dps[MPU6050_GYRO_CONF_FS_MAX] = {
	[MPU6050_GYRO_CONF_FS_250_DPS] = 250,
	[MPU6050_GYRO_CONF_FS_500_DPS] = 500,
	[MPU6050_GYRO_CONF_FS_1000_DPS] = 1000,
	[MPU6050_GYRO_CONF_FS_2000_DPS] = 2000,
};

/**
 * @brief Actual gyroscope full-scale range, in dps, for a given FS_SEL.
 */
uint32_t mpu6050_fs_to_dps(mpu6050_gyro_fs_t fs)
{
	return mpu6050_gyro_fs_range_dps[fs];
}

/**
 * @brief Gyroscope sensitivity, in LSB per degree/s, for a given full-scale range.
 */
double mpu6050_gyro_fs_to_sensitivity(mpu6050_gyro_fs_t fs)
{
	return MPU6050_ADC_FS_COUNTS / (double)mpu6050_gyro_fs_range_dps[fs];
}

mpu6050_gyro_fs_t mpu6050_dps_to_fs(int32_t dps)
{
	for (mpu6050_gyro_fs_t fs = MPU6050_GYRO_CONF_FS_250_DPS; fs < MPU6050_GYRO_CONF_FS_MAX;
	     fs++) {
		if (dps < mpu6050_gyro_fs_range_dps[fs]) {
			return fs;
		}
	}

	return MPU6050_GYRO_CONF_FS_MAX;
}

double mpu6050_mG_to_ms2(uint32_t mG)
{
	return ((double)mG / 1000.0) * ((double)SENSOR_G / 1000000.0);
}

uint32_t mpu6050_ms2_to_mG(double ms2)
{
	return (uint32_t)((ms2 / ((double)SENSOR_G / 1000000.0)) * 1000.0);
}

double mpu6050_dps_to_rad_s(uint32_t dps)
{
	return (double)dps * ((double)SENSOR_PI / 1000000.0) / 180.0;
}

uint32_t mpu6050_rad_s_to_dps(double rad_s)
{
	return (uint32_t)(rad_s * 180.0 / ((double)SENSOR_PI / 1000000.0));
}
