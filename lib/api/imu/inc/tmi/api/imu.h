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
	tmi_imu_vec3_t accel_iir; /** Accelerometer raw IIR filtered data. */
	tmi_imu_vec3_t gyro_iir;  /** Gyroscope raw IIR filtered data. */
} tmi_imu_data_t;

typedef struct tmi_imu_s tmi_imu_t;

typedef struct {
	int (*init)(tmi_imu_t *dev);
	int (*get_accel)(tmi_imu_t *dev, tmi_imu_vec3_t *raw);
	int (*get_gyro)(tmi_imu_t *dev, tmi_imu_vec3_t *raw);
	int (*set_accel_fs_mG)(tmi_imu_t *dev, uint32_t mG);
	int (*set_gyro_fs_dps)(tmi_imu_t *dev, uint32_t dps);
	int (*get_temp_mC)(tmi_imu_t *dev, int16_t *mC);
} tmi_imu_api_t;

struct tmi_imu_s {
	tmi_imu_config_t config;
	tmi_imu_data_t data;
	const tmi_imu_api_t *api;
};

static inline int tmi_imu_init(tmi_imu_t *dev)
{
	return dev->api->init(dev);
}

static inline int tmi_imu_get_accel(tmi_imu_t *dev, tmi_imu_vec3_t *raw)
{
	return dev->api->get_accel(dev, raw);
}

static inline int tmi_imu_get_gyro(tmi_imu_t *dev, tmi_imu_vec3_t *raw)
{
	return dev->api->get_gyro(dev, raw);
}

static inline int tmi_imu_get_temp_mC(tmi_imu_t *dev, int16_t *mC)
{
	return dev->api->get_temp_mC(dev, mC);
}

static inline int tmi_imu_set_accel_fs_mG(tmi_imu_t *dev, uint32_t mG)
{
	return dev->api->set_accel_fs_mG(dev, mG);
}

static inline int tmi_imu_set_gyro_fs_dps(tmi_imu_t *dev, uint32_t dps)
{
	return dev->api->set_gyro_fs_dps(dev, dps);
}

#endif /* TMI_API_IMU_H */
