
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <stdint.h>

#ifdef CONFIG_TMI_DRIVER_TCS3400_TRIGGER
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#endif

#define TCS3400_REG_ENABLE                       0x80
#define TCS3400_REG_RGBC_INTEGRATION             0x81
#define TCS3400_REG_WAIT_TIME                    0x83
#define TCS3400_REG_CLEAR_INT_LOW_THRES_LO       0x84
#define TCS3400_REG_CLEAR_INT_LOW_THRES_HI       0x85
#define TCS3400_REG_CLEAR_INT_HIGH_THRES_LO      0x86
#define TCS3400_REG_CLEAR_INT_HIGH_THRES_HI      0x87
#define TCS3400_REG_INTERRUPT_PERSISTENCE_FILTER 0x8C
#define TCS3400_REG_CONFIG                       0x8D
#define TCS3400_REG_CTRL                         0x8F
#define TCS3400_REG_AUX                          0x90
#define TCS3400_REG_REVID                        0x91
#define TCS3400_REG_ID                           0x92
#define TCS3400_REG_STATUS
#define TCS3400_REG_CDATAL 0x94
#define TCS3400_REG_CDATAH 0x95
#define TCS3400_REG_RDATAL 0x96
#define TCS3400_REG_RDATAH 0x97
#define TCS3400_REG_GDATAL 0x98
#define TCS3400_REG_GDATAH 0x99
#define TCS3400_REG_BDATAL 0x9A
#define TCS3400_REG_BDATAH 0x9B

#define TCS3400_REG_IR      0xC0
#define TCS3400_REG_IFORCE  0xE4
#define TCS3400_REG_CICLEAR 0xE6
#define TCS3400_REG_AICLEAR 0xE7

#define TCS3400_MASK_INT_ENABLE_DATA_RDY_EN 0x13
#define TCS3400_MASK_CTRL_AGAIN             0x03

/* Default values listed by manufactuer upon booting the device */
#define TCS3400_DEFAULT_ENABLE 0x00
#define TCS3400_DEFAULT_ATIME  0xff
#define TCS3400_DEFAULT_PERS   0x00
#define TCS3400_DEFAULT_CONFIG 0x00
#define TCS3400_DEFAULT_CTRL   0x00

/*Writing 0x00 will reset the device*/
#define TCS3400_AICLEAR_RESET 0x00

#define CHECK_NULL_PTR(ptr)                                                                        \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR("%s: null pointer: " #ptr, __func__);                              \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

typedef enum {
	TCS3400_CONF_FS_1x_DPS = 0,
	TCS3400_CONF_FS_4x_DPS = 1,
	TCS3400_CONF_FS_16x_DPS = 2,
	TCS3400_CONF_FS_64x_DPS = 3,
	TCS3400_CONF_FS_MAX = 4,
} tcs3400_color_gain_t;

typedef enum {
	TCS3400_CONF_CYCLES_1 = 0xFF,
	TCS3400_CONF_CYCLES_10 = 0xF6,
	TCS3400_CONF_CYCLES_37 = 0xDB,
	TCS3400_CONF_CYCLES_64 = 0xC0,
	TCS3400_CONF_CYCLES_256_MAX = 0x00,
} tcs3400_integration_cycles_t;

typedef struct {
	struct i2c_dt_spec i2c;
#ifdef CONFIG_TMI_DRIVER_TCS3400_TRIGGER
	struct gpio_dt_spec int_gpio;
#endif
} tcs3400_config_t;

typedef struct {
	struct sensor_value colors[4];
	tcs3400_color_gain_t gain;
	tcs3400_integration_cycles_t integration_cycles;
#ifdef CONFIG_TMI_DRIVER_TCS3400_TRIGGER
	const struct device *dev;
	struct gpio_callback gpio_cb;

	const struct sensor_trigger *data_ready_trigger;
	sensor_trigger_handler_t data_ready_handler;

#if defined(CONFIG_TMI_DRIVER_TCS3400_TRIGGER_OWN_THREAD)
	struct k_sem gpio_sem;
	K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_TMI_DRIVER_TCS3400_THREAD_STACK_SIZE);
	struct k_thread thread;
#elif defined(CONFIG_TMI_DRIVER_TCS3400_TRIGGER_GLOBAL_THREAD)
	struct k_work work;
#endif
#endif
} tcs3400_data_t;

// bus
int tcs3400_read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len);
int tcs3400_write_reg(const struct device *dev, uint8_t reg, uint8_t val);
int tcs3400_write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val);

// utils
tcs3400_color_gain_t tcs3400_scale_gain(uint32_t scale);
tcs3400_integration_cycles_t tcs3400_scale_integration(uint32_t cycles);

// trigger
#ifdef CONFIG_TMI_DRIVER_TCS3400_TRIGGER
int tcs3400_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler);
int tcs3400_init_interrupt(const struct device *dev);
#endif /* CONFIG_TMI_DRIVER_MPU6050_TRIGGER */
