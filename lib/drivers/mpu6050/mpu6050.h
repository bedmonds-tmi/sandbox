/**
 * @file mpu6050.h
 * @brief Register map, device config/data types, and internal API shared
 * across the MPU6050 driver's source files.
 *
 * @details Not a public driver API header; included only by this driver's
 * own .c files (and devicetree-generated code via @ref DEVICE_DT_INST_DEFINE).
 * Declares the register/bitmask definitions, the FS_SEL enums, the
 * mpu6050_config_t/mpu6050_data_t device instance types, and the internal
 * functions implemented in mpu6050_bus.c, mpu6050_utils.c, and
 * mpu6050_trigger.c.
 */

#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <stdint.h>

#ifdef CONFIG_TMI_DRIVER_MPU6050_TRIGGER
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#endif

#define MPU6050_I2C_ADDR0 0x68
#define MPU6050_I2C_ADDR1 0x69

#define MPU6050_REG_ACCEL_XOUTH  0x3B
#define MPU6050_REG_TEMP_OUTH    0x41
#define MPU6050_REG_GYRO_XOUTH   0x43
#define MPU6050_REG_WHOAMI       0x75
#define MPU6050_REG_USER_CTRL    0x6A
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_SELF_TEST_X  0x0D
#define MPU6050_REG_SELF_TEST_Y  0x0E
#define MPU6050_REG_SELF_TEST_Z  0x0F
#define MPU6050_REG_SELF_TEST_A  0x10
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_MOT_THR      0x1F
#define MPU6050_REG_FIFO_EN      0x23
#define MPU6050_REG_INT_PIN_CFG  0x37
#define MPU6050_REG_INT_ENABLE   0x38
#define MPU6050_REG_INT_STATUS   0x3A

#define MPU6050_ACCEL_DATA_LEN 6U
#define MPU6050_TEMP_DATA_LEN  2U
#define MPU6050_GYRO_DATA_LEN  6U

/**
 * GENMASK(4, 3) is the same as (0x03 << 3), so bits 3 and 4 are set
 * (0b00011000). BIT(5) is the same as (1 << 5), so bit 5 is set
 * (0b00100000).
 */
#define MPU6050_MASK_ACCEL_CONFIG_AFS_SEL GENMASK(4, 3)
#define MPU6050_MASK_ACCEL_CONFIG_ZA_ST   BIT(5)
#define MPU6050_MASK_ACCEL_CONFIG_YA_ST   BIT(6)
#define MPU6050_MASK_ACCEL_CONFIG_XA_ST   BIT(7)

#define MPU6050_MASK_GYRO_CONFIG_FS_SEL GENMASK(4, 3)
#define MPU6050_MASK_GYRO_CONFIG_ZG_ST  BIT(5)
#define MPU6050_MASK_GYRO_CONFIG_YG_ST  BIT(6)
#define MPU6050_MASK_GYRO_CONFIG_XG_ST  BIT(7)

#define MPU6050_MASK_PWR_MGMT_1_SLEEP BIT(6)

#define MPU6050_MASK_INT_ENABLE_DATA_RDY_EN BIT(0)

#define CHECK_NULL_PTR(ptr)                                                                        \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR("%s: null pointer: " #ptr, __func__);                              \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

/**
 * @brief Full scale accel conf selections
 */
typedef enum {
	MPU6050_ACCEL_CONF_FS_2_G = 0,
	MPU6050_ACCEL_CONF_FS_4_G = 1,
	MPU6050_ACCEL_CONF_FS_8_G = 2,
	MPU6050_ACCEL_CONF_FS_16_G = 3,
	MPU6050_ACCEL_CONF_FS_MAX = 4,
} mpu6050_accel_fs_t;

/**
 * @brief Full scale gyro conf selections
 */
typedef enum {
	MPU6050_GYRO_CONF_FS_250_DPS = 0,
	MPU6050_GYRO_CONF_FS_500_DPS = 1,
	MPU6050_GYRO_CONF_FS_1000_DPS = 2,
	MPU6050_GYRO_CONF_FS_2000_DPS = 3,
	MPU6050_GYRO_CONF_FS_MAX = 4,
} mpu6050_gyro_fs_t;

typedef struct {
	struct i2c_dt_spec i2c;
	uint16_t accel_fs_mG;
	uint16_t gyro_fs_dps;
#ifdef CONFIG_TMI_DRIVER_MPU6050_TRIGGER
	struct gpio_dt_spec int_gpio;
#endif
} mpu6050_config_t;

typedef struct {
	mpu6050_accel_fs_t accel_fs; /** Currently configured accelerometer full-scale range. */
	mpu6050_gyro_fs_t gyro_fs;   /** Currently configured gyroscope full-scale range. */
	struct sensor_value accel[3];
	struct sensor_value gyro[3];
#ifdef CONFIG_TMI_DRIVER_MPU6050_TRIGGER
	const struct device *dev;
	struct gpio_callback gpio_cb;

	const struct sensor_trigger *data_ready_trigger;
	sensor_trigger_handler_t data_ready_handler;

#if defined(CONFIG_TMI_DRIVER_MPU6050_TRIGGER_OWN_THREAD)
	struct k_sem gpio_sem;
	K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_TMI_DRIVER_MPU6050_THREAD_STACK_SIZE);
	struct k_thread thread;
#elif defined(CONFIG_TMI_DRIVER_MPU6050_TRIGGER_GLOBAL_THREAD)
	struct k_work work;
#endif
#endif /* CONFIG_TMI_DRIVER_MPU6050_TRIGGER */
} mpu6050_data_t;

// Bus
int mpu6050_read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len);
int mpu6050_write_reg(const struct device *dev, uint8_t reg, uint8_t val);
int mpu6050_write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val);

// Utils
uint32_t mpu6050_fs_to_mG(mpu6050_accel_fs_t fs);
mpu6050_accel_fs_t mpu6050_mG_to_fs(uint32_t mG);
double mpu6050_accel_fs_to_sensitivity(mpu6050_accel_fs_t fs);

uint32_t mpu6050_fs_to_dps(mpu6050_gyro_fs_t fs);
mpu6050_gyro_fs_t mpu6050_dps_to_fs(int32_t dps);
double mpu6050_gyro_fs_to_sensitivity(mpu6050_gyro_fs_t fs);

double mpu6050_mG_to_ms2(uint32_t mG);
uint32_t mpu6050_ms2_to_mG(double ms2);
double mpu6050_dps_to_rad_s(uint32_t dps);
uint32_t mpu6050_rad_s_to_dps(double rad_s);

// Trigger
#ifdef CONFIG_TMI_DRIVER_MPU6050_TRIGGER
int mpu6050_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler);
int mpu6050_init_interrupt(const struct device *dev);
#endif /* CONFIG_TMI_DRIVER_MPU6050_TRIGGER */
