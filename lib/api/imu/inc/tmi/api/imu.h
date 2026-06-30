#ifndef TMI_API_IMU_H
#define TMI_API_IMU_H

#include <stdint.h>
#include <zephyr/device.h>

typedef union {
	uint8_t bytes[6];
	struct {
		int16_t x;
		int16_t y;
		int16_t z;
	};
} tmi_imu_vec3_t;

typedef struct {
	const struct device *bus;
	uint8_t addr;
	uint8_t alpha_div;    /** IIR filter alpha divisor. */
	uint32_t accel_fs_mG; /** Accelerometer full scale range in mill-gravity. */
	uint32_t gyro_fs_dps; /** Gyroscope full scale range in degrees-per-second. */
} tmi_imu_config_t;

typedef struct {
	tmi_imu_config_t config; /** Device configuration. */
	tmi_imu_vec3_t accel_iir; /** Accelerometer raw IIR filtered data. */
	tmi_imu_vec3_t gyro_iir;  /** Gyroscope raw IIR filtered data. */
} tmi_imu_data_t;

typedef struct {
	int (*init)(const struct device *dev);
	int (*get_accel)(const struct device *dev, tmi_imu_vec3_t *raw);
	int (*get_gyro)(const struct device *dev, tmi_imu_vec3_t *raw);
	int (*set_accel_fs_mG)(const struct device *dev, uint32_t mG);
	int (*set_gyro_fs_dps)(const struct device *dev, uint32_t dps);
	int (*get_temp_mC)(const struct device *dev, int16_t *mC);
} tmi_imu_api_t;

static inline int tmi_imu_init(const struct device *dev)
{
	const tmi_imu_api_t *api = (const struct tmi_imu_api_t *)dev->api;
	return api->init(dev);
}

static inline int tmi_imu_get_accel(const struct device *dev, tmi_imu_vec3_t *raw)
{
	const tmi_imu_api_t *api = (const struct tmi_imu_api_t *)dev->api;
	return api->get_accel(dev, raw);
}

static inline int tmi_imu_get_gyro(const struct device *dev, tmi_imu_vec3_t *raw)
{
	const tmi_imu_api_t *api = (const struct tmi_imu_api_t *)dev->api;
	return api->get_gyro(dev, raw);
}

static inline int tmi_imu_get_temp_mC(const struct device *dev, int16_t *mC)
{
	const tmi_imu_api_t *api = (const struct tmi_imu_api_t *)dev->api;
	return api->get_temp_mC(dev, mC);
}

static inline int tmi_imu_set_accel_fs_mG(const struct device *dev, uint32_t mG)
{
	const tmi_imu_api_t *api = (const struct tmi_imu_api_t *)dev->api;
	return api->set_accel_fs_mG(dev, mG);
}

static inline int tmi_imu_set_gyro_fs_dps(const struct device *dev, uint32_t dps)
{
	const tmi_imu_api_t *api = (const struct tmi_imu_api_t *)dev->api;
	return api->set_gyro_fs_dps(dev, dps);
}

#endif /* TMI_API_IMU_H */
