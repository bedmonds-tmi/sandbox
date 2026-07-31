
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

#define TCS3400_REGISTER_ENABLE                               0x80
#define TCS3400_REGISTER_RGBC_INTEGRATION                     0x81
#define TCS3400_REGISTER_WAIT_TIME                            0x83
#define TCS3400_REGISTER_CLEAR_INTERRUPT_LOW_THRES_LOW_BYTE   0x84
#define TCS3400_REGISTER_CLEAR_INTERRUPT_LOW_THRES_HIGH_BYTE  0x85
#define TCS3400_REGISTER_CLEAR_INTERRUPT_HIGH_THRES_LOW_BYTE  0x86
#define TCS3400_REGISTER_CLEAR_INTERRUPT_HIGH_THRES_HIGH_BYTE 0x87
#define TCS3400_REGISTER_INTERRUPT_PERSISTENCE_FILTER         0x8C
#define TCS3400_REGISTER_CONFIG                               0x8D
#define TCS3400_REGISTER_CONTROL                              0x8F
#define TCS3400_REGISTER_AUX                                  0x90
#define TCS3400_REGISTER_REVID                                0x91
#define TCS3400_REGISTER_ID                                   0x92
#define TCS3400_REGISTER_STATUS                               0x93
#define TCS3400_REGISTER_CDATAL                               0x94
#define TCS3400_REGISTER_CDATAH                               0x95
#define TCS3400_REGISTER_RDATAL                               0x96
#define TCS3400_REGISTER_RDATAH                               0x97
#define TCS3400_REGISTER_GDATAL                               0x98
#define TCS3400_REGISTER_GDATAH                               0x99
#define TCS3400_REGISTER_BDATAL                               0x9A
#define TCS3400_REGISTER_BDATAH                               0x9B
#define TCS3400_REGISTER_IR                                   0xC0
#define TCS3400_REGISTER_IFORCE                               0xE4
#define TCS3400_REGISTER_CICLEAR                              0xE6
#define TCS3400_REGISTER_AICLEAR                              0xE7

#define CHECK_NULL_PTR(ptr)                                                                        \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR("%s: null pointer: " #ptr, __func__);                              \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

typedef struct {
	struct i2c_dt_spec i2c;
} tcs3400d_config_t;

typedef struct {
	tcs3400_config_t config;
	tcs3400_color_t colors;
} tcs3400_data_t;

typedef union {
	uint8_t bytes[6];
	struct {
		uint32_t red;
		uint32_t blue;
		uint32_t green;
	}
} tcs3400_color_t
