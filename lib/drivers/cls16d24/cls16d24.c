#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "cls16d24.h"

LOG_MODULE_REGISTER(cls16d24);

#define CLS16D24_REG_SYSM_CTRL   0x00
#define CLS16D24_REG_INT_FLAG    0x02
#define CLS16D24_REG_WAIT_TIME   0x03
#define CLS16D24_REG_CLS_GAIN    0x04
#define CLS16D24_REG_CLS_TIME    0x05
#define CLS16D24_REG_PROD_ID_L   0xBC
#define CLS16D24_REG_PROD_ID_H   0xBD
#define CLS16D24_REG_INT_SOURCE  0x16
#define CLS16D24_REG_ERROR_FLAG  0x17
#define CLS16D24_REG_RCH_DATA_L  0x1C
#define CLS16D24_REG_RCH_DATA_H  0x1D
#define CLS16D24_REG_GCH_DATA_L  0x1E
#define CLS16D24_REG_GCH_DATA_H  0x1F
#define CLS16D24_REG_BCH_DATA_L  0x20
#define CLS16D24_REG_BCH_DATA_H  0x21
#define CLS16D24_REG_WCH_DATA_L  0x22
#define CLS16D24_REG_WCH_DATA_H  0x23
#define CLS16D24_REG_IRCH_DATA_L 0x24
#define CLS16D24_REG_IRCH_DATA_H 0x25

#define CLS16D24_REG_CLS_THRE_LL 0x0C
#define CLS16D24_REG_CLS_THRE_LH 0x0D
#define CLS16D24_REG_CLS_THRE_HL 0x0E
#define CLS16D24_REG_CLS_THRE_HH 0x0F

#define CLS16D24_RGBW_DATA_LEN 8U
#define CLS16D24_IR_DATA_LEN   2U

#define CLS16D24_BIT_SYSM_CTRL_EN_CLS  BIT(0)
#define CLS16D24_BIT_SYSM_CTRL_EN_IR   BIT(1)
#define CLS16D24_BIT_SYSM_CTRL_EN_WAIT BIT(6)
#define CLS16D24_BIT_SYSM_CTRL_SWSRT   BIT(7)

#define CLS16D24_BIT_INT_CTRL_EN_CINT BIT(0)
#define CLS16D24_BIT_INT_CTRLCLS_SYNC BIT(5)

#define CLS16D24_BIT_INT_FLAG_INT_CLS   BIT(0)
#define CLS16D24_BIT_INT_FLAG_DATA_FLAG BIT(6)
#define CLS16D24_BIT_INT_FLAG_INT_POR   BIT(7)

#define CLS16D24_BIT_CLS_GAIN_PGA_CLS   (0x1F)
#define CLS16D24_BIT_CLS_GAIN_DIOD_SELT BIT(8)

#define CLS16D24_BIT_CLS_TIME_INT_TIME (0x03)
#define CLS16D24_BIT_CLS_TIME_CLSCONV  (0xF << 4)

#define CLS16D24_BIT_PERSISTENCE_PRS_CLS (0xF)

#define CLS16D24_BIT_INT_SOURCE_INT_SRC (0x1F)

#define CLS16D24_BIT_ERROR_FLAG_ERR_RCH  BIT(0)
#define CLS16D24_BIT_ERROR_FLAG_ERR_GCH  BIT(1)
#define CLS16D24_BIT_ERROR_FLAG_ERR_BCH  BIT(2)
#define CLS16D24_BIT_ERROR_FLAG_ERR_WCH  BIT(3)
#define CLS16D24_BIT_ERROR_FLAG_ERR_IRCH BIT(4)

#define CHECK_NULL_PTR(ptr)                                                                        \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			LOG_ERR("%s: null pointer: " #ptr, __func__);                              \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

static int read_reg(const struct device *dev, uint8_t reg, uint8_t *val, uint8_t len)
{
	const cls16d24_config_t *cfg = (const cls16d24_config_t *)dev->config;
	return i2c_write_read_dt(&cfg->i2c, &reg, 1, val, len);
}

static int write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	const cls16d24_config_t *cfg = (const cls16d24_config_t *)dev->config;
	return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

static int write_mask(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t tmp;

	int ret = read_reg(dev, reg, &tmp, sizeof(tmp));
	if (ret != 0) {
		LOG_ERR("Error: %d", ret);
		return ret;
	}

	tmp &= ~mask;                 // Clear the target bit
	tmp |= FIELD_PREP(mask, val); // Set the target bit based on val

	return write_reg(dev, reg, tmp);
}

/**
 * @brief Verify communication with the CLS16D24.
 *
 * @details Reads the WHOAMI register and checks that it matches the configured
 * I2C address.
 *
 * @param[in] dev CLS16D24 device instance. Must not be NULL.
 *
 * @retval 0 The expected WHOAMI value was read.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -ERANGE If the WHOAMI value does not match the configured address.
 * @return Negative errno from the register read operation on failure.
 */
static int cls16d24_check_whoami(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	const cls16d24_config_t *cfg = (const cls16d24_config_t *)dev->config;

	uint8_t x;
	int ret = read_reg(dev, CLS16D24_REG_PROD_ID_L, &x, sizeof(x));
	if (ret != 0) {
		LOG_ERR("Read reg failed when reading WHOAMI reg: %d", ret);
		return ret;
	}

	/* WHOAMI value will be equal to the address. */
	if (x != cfg->i2c.addr) {
		LOG_ERR("WHOAMI mismatch. Got 0x%02X, expected 0x%02X", x, cfg->i2c.addr);
		return -ERANGE;
	}

	return 0;
}

static int cls16d24_get_rgbw(const struct device *dev, tmi_light_rgbw_t *raw)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(raw);

	mcls16d24_data_t *data = (cls16d24_data_t *)dev->data;

	uint8_t tmp[8];
	int ret = read_reg(dev, CLS16D24_REG_RCH_DATA_L, tmp, sizeof(tmp));
	if (ret != 0) {
		return ret;
	}

	raw->r = sys_get_be16(&tmp[0]);
	raw->g = sys_get_be16(&tmp[2]);
	raw->b = sys_get_be16(&tmp[4]);
	raw->w = sys_get_be16(&tmp[6]);

	data->r = raw->r;
	data->g = raw->g;
	data->b = raw->b;
	data->w = raw->w;

	return 0;
}

static int cls16d24_get_ir(const struct device *dev, tmi_light_ir_t *raw)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(raw);

	cls16d24_data_t *data = (cls16d24_data_t *)dev->data;

	uint8_t tmp[0];
	read_reg(dev, CLS16D24_REG_IRCH_DATA_L, tmp, sizeof(tmp));

	raw->ir = sys_get_be16(&tmp[0]);

	data->ir = raw->ir;

	return 0;
}

static cls16d24_en_ir_fs_t cls16d24_en_ir_to_fs_sel(int32_t ir)
{
	if (ir = 0) {
		return CLS16D24_IR_CONF_FS_DISABLE;
	} else if (ir = 1) {
		return CLS16D24_IR_CONF_FS_ENABLE;
	} else {
		LOG_WRN("CLS16D24 can't set %d en_ir, setting to default", ir);
		return CLS16D24_IR_CONF_FS_DISABLE;
	}
}

static int cls16d24_set_en_ir_fs_ir(const struct device *dev, uint32_t ir)
{
	CHECK_NULL_PTR(dev);

	cls16d24_data_t *data = (cls16d24_data_t *)dev->data;

	cls16d24_int_time_fs_t en_ir_fs = ccls16d24_en_ir_to_fs_sel(ir);

	int ret = write_mask(dev, CLS16D24_REG_SYSM_CTRL, CLS16D24_BIT_SYSM_CTRL_EN_IR,
			     (uint8_t)en_ir_fs);
	if (ret != 0) {
		return ret;
	}

	data->config.en_ir_fs_ir = ir;

	return 0;
}

static cls16d24_gain_fs_t cls16d24_gain_to_fs_sel(int32_t x)
{
	if (x < 1) {
		return CLS16D24_GAIN_CONF_FS_X1;
	} else if (x < 4) {
		return CLS16D24_GAIN_CONF_FS_X4;
	} else if (x < 8) {
		return CLS16D24_GAIN_CONF_FS_X8;
	} else if (x < 32) {
		return CLS16D24_GAIN_CONF_FS_X32;
	} else if (x < 96) {
		return CLS16D24_GAIN_CONF_FS_X96;
	} else {
		LOG_WRN("CLS16D24 can't achieve %d gain, clamping to X96", x);
		return CLS16D24_GAIN_CONF_FS_X96;
	}
}

static int cls16d24_set_gain_fs_x(const struct device *dev, uint32_t x)
{
	CHECK_NULL_PTR(dev);

	cls16d24_data_t *data = (cls16d24_data_t *)dev->data;

	cls16d24_gain_fs_t gain_fs = cls16d24_gain_to_fs_sel(x);

	int ret = write_mask(dev, CLS16D24_REG_CLS_GAIN, CLS16D24_BIT_CLS_GAIN_PGA_CLS,
			     (uint8_t)gain_fs);
	if (ret != 0) {
		return ret;
	}

	data->config.gain_fs_x = x;

	return 0;
}

static cls16d24_int_time_fs_t cls16d24_int_time_to_fs_sel(int32_t time)
{
	if (time < 1) {
		return CLS16D24_INT_TIME_CONF_FS_T;
	} else if (time < 4) {
		return CLS16D24_INT_TIME_CONF_FS_4T;
	} else if (time < 16) {
		return CLS16D24_INT_TIME_CONF_FS_16T;
	} else if (time < 64) {
		return CLS16D24_INT_TIME_CONF_FS_64T;
	} else {
		LOG_WRN("CLS16D24 can't achieve %d int time, clamping to 64T", time);
		return CLS16D24_INT_TIME_CONF_FS_64T;
	}
}

static int cls16d24_set_int_time_fs_time(const struct device *dev, uint32_t time)
{
	CHECK_NULL_PTR(dev);

	cls16d24_data_t *data = (cls16d24_data_t *)dev->data;

	cls16d24_int_time_fs_t int_time_fs = cls16d24_int_time_to_fs_sel(time);

	int ret = write_mask(dev, CLS16D24_REG_CLS_TIME, CLS16D24_BIT_CLS_TIME_INT_TIME,
			     (uint8_t)int_time_fs);
	if (ret != 0) {
		return ret;
	}

	data->config.int_time_fs_time = time;

	return 0;
}

static cls16d24_int_src_fs_t cls16d24_int_src_to_fs_sel(int32_t dt)
{
	if (dt = 0) {
		return CLS16D24_INT_SRC_CONF_FS_RCH_DATA;
	} else if (dt = 1) {
		return CLS16D24_INT_SRC_CONF_FS_GCH_DATA;
	} else if (dt = 2) {
		return CLS16D24_INT_SRC_CONF_FS_BCG_DATA;
	} else if (dt = 3) {
		return CLS16D24_INT_SRC_CONF_FS_WCH_DATA;
	} else if (dt = 4) {
		return CLS16D24_INT_SRC_CONF_FS_IRCH_DATA;
	} else {
		LOG_WRN("CLS16D24 can't set %d int src, setting to default - WCH", dt);
		return CLS16D24_INT_SRC_CONF_FS_WCH_DATA;
	}
}

static int cls16d24_set_int_src_fs_dt(const struct device *dev, uint32_t dt)
{
	CHECK_NULL_PTR(dev);

	cls16d24_data_t *data = (cls16d24_data_t *)dev->data;

	cls16d24_int_src_fs_t int_src_fs = cls16d24_int_src_to_fs_sel(dt);

	int ret = write_mask(dev, CLS16D24_REG_INT_SOURCE, CLS16D24_BIT_INT_SOURCE_INT_SRC,
			     (uint8_t)int_src_fs);
	if (ret != 0) {
		return ret;
	}

	data->config.int_src_fs_dt = dt;

	return 0;
}

static cls16d24_diode_selt_fs_t cls16d24_diode_selt_to_fs_sel(int32_t x)
{
	if (x = 0) {
		return CLS16D24_DIODE_SELT_CONF_FS_X1;
	} else if (x = 1) {
		return CLS16D24_DIODE_SELT_CONF_FS_X2;
	} else {
		LOG_WRN("CLS16D24 can't set %d int src, setting to default", x);
		return CLS16D24_DIODE_SELT_CONF_FS_X2;
	}
}

static int cls16d24_set_diode_selt_fs_x(const struct device *dev, uint32_t x)
{
	CHECK_NULL_PTR(dev);

	cls16d24_data_t *data = (cls16d24_data_t *)dev->data;

	cls16d24_int_time_fs_t diode_selt_fs = cls16d24_diode_selt_to_fs_sel(x);

	int ret = write_mask(dev, CLS16D24_REG_CLS_GAIN, CLS16D24_BIT_CLS_GAIN_DIOD_SELT,
			     (uint8_t)diode_selt_fs);
	if (ret != 0) {
		return ret;
	}

	data->config.diode_selt_fs_x = x;

	return 0;
}

static int cls16d24_init(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	const cls16d24_config_t *cfg = (const cls16d24_config_t *)dev->config;
	cls16d24_data_t *data = (cls16d24_data_t *)dev->data;

	if (dev == NULL) {
		LOG_ERR("Null pointer to device.");
		return -EINVAL;
	}

	bool ready = i2c_is_ready_dt(&cfg->i2c);
	if (!ready) {
		LOG_ERR("I2C bus is not ready.");
		return -EFAULT;
	}

	int ret = cls16d24_check_whoami(dev);
	if (ret != 0) {
		return -ERANGE;
	}

	ret = write_mask(dev, CLS16D24_REG_SYSM_CTRL, CLS16D24_BIT_SYSM_CTRL_EN_CLS, 1);
	if (ret != 0) {
		LOG_ERR("Failed to write to wake device: %d", ret);
		return ret;
	}

	ret = cls16d24_set_en_ir_fs_ir(dev, cfg->en_ir_fs_ir);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set en_ir fs (Err %d).", ret);
		return ret;
	}

	ret = cls16d24_set_gain_fs_x(dev, cfg->gain_fs_x);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set gain fs (Err %d).", ret);
		return ret;
	}

	ret = cls16d24_set_int_time_fs_time(dev, cfg->int_time_fs_time);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set int_time fs (Err %d).", ret);
		return ret;
	}

	ret = cls16d24_set_int_src_fs_dt(dev, cfg->int_src_fs_dt);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set int_src fs (Err %d).", ret);
		return ret;
	}

	ret = cls16d24_set_diode_selt_fs_x(dev, cfg->diode_selt_fs_x);
	if (ret != 0) {
		LOG_ERR("Init failed, could not set diode_selt fs (Err %d).", ret);
		return ret;
	}

	ret = cls16d24_get_rgbw(dev, &rgbw);
	if (ret != 0) {
		return ret;
	}

	ret = cls16d24_get_ir(dev, &ir);
	if (ret != 0) {
		return ret;
	}

	data = rgbw;
	data = ir;

	LOG_INF("Initialized");
	return 0;
}

static const tmi_light_api_t cls16d24_api = {
	.init = cls16d24_init,
	.get_rgbw = cls16d24_get_rgbw,
	.get_ir = cls16d24_get_ir,
	.set_en_ir_fs_ir = cls16d24_set_en_ir_fs_ir,
	.set_gain_fs_x = cls16d24_set_gain_fs_x,
	.set_int_time_fs_time = cls16d24_set_int_time_fs_time,
	.set_int_src_fs_dt = cls16d24_set_int_src_fs_dt,
	.set_diode_selt_fs_x = cls16d24_set_diode_selt_fs_x,
};

#define DT_DRV_COMPAT tmi_cls16d24

#define CLS16D24_DEFINE(inst)                                                                      \
	static cls16d24_data_t cls16d24_data_##inst;                                               \
                                                                                                   \
	static const cls16d24_config_t cls16d24_config_##inst = {                                  \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.en_ir_fs_ir = DT_INST_PROP(inst, en_ir_fs_ir),                                    \
		.gain_fs_x = DT_INST_PROP(inst, gain_fs_x),                                        \
		.int_time_fs_time = DT_INST_PROP(inst, int_time_fs_time),                          \
		.int_src_fs_dt = DT_INST_PROP(inst, int_src_fs_dt),                                \
		.diode_selt_fs_x = DT_INST_PROP(inst, diode_selt_fs_x),                            \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, cls16d24_init, NULL, &cls16d24_data_##inst,                    \
			      &cls16d24_config_##inst, POST_KERNEL,                                \
			      CONFIG_TMI_DRIVER_CLS16D24_INIT_PRIORITY, &cls16d24_api);

DT_INST_FOREACH_STATUS_OKAY(CLS16D24_DEFINE)
