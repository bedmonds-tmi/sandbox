#include "cls16d24.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

#define CLS_16D24_44_DF8_TR8        0x38
#define REG_SYSM_CTRL               0x00
#define REG_INT_FLAG                0x02
#define WAIT_TIME                   0x03
#define CLS_GAIN                    0x04
#define CLS_TIME                    0x05
#define PROD_ID_L                   0xBC
#define PROD_ID_H                   0xBD
#define ERROR_FLAG                  0x17
#define RCH_DATA_L                  0x1C
#define RCH_DATA_H                  0x1D
#define GCH_DATA_L                  0x1E
#define GCH_DATA_H                  0x1F
#define BCH_DATA_L                  0x20
#define BCH_DATA_H                  0x21
#define WCH_DATA_L                  0x22
#define WCH_DATA_H                  0x23
#define IRCH_DATA_L                 0x24
#define IRCH_DATA_H                 0x25

#define CLS_THRE_LL                 0x0C
#define CLS_THRE_LH                 0x0D
#define CLS_THRE_HL                 0x0E
#define CLS_THRE_HH                 0x0F

#define BIT_SYSM_CTRLEN_CLS BIT     (0)
#define BIT_SYSM_CTRL_EN_IR BIT     (1)
#define BIT_SYSM_CTRL_EN_WAIT BIT   (6)
#define BIT_SYSM_CTRL_SWSRT BIT     (7)

#define BIT_INT_CTRL_EN_CINT BIT    (0)
#define BIT_INT_CTRLCLS_SYNC BIT    (5)


#define BIT_INT_FLAG_INT_CLS BIT    (0)
#define BIT_INT_FLAG_DATA_FLAG BIT  (6)
#define BIT_INT_FLAG_INT_POR BIT    (7)

#define BIT_CLS_GAIN_PGA_CLS        (0x1F)
#define BIT_CLS_GAIN_DIOD_SELT BIT  (8)

#define BIT_CLS_TIME_INT_TIME       (0x03)
#define BIT_CLS_TIME_CLSCONV        (0xF << 4)

#define BIT_PERSISTENCE_PRS_CLS     (0xF)

#define BIT_INT_SOURCE_INT_SRC      (0x1F)

#define BIT_ERROR_FLAG_ERR_RCH BIT      (0)
#define BIT_ERROR_FLAG_ERR_GCH BIT      (1)
#define BIT_ERROR_FLAG_ERR_BCH BIT      (2)
#define BIT_ERROR_FLAG_ERR_WCH BIT      (3)
#define BIT_ERROR_FLAG_ERR_IRCH BIT     (4)

#define I2C_NODE DT_NODELABEL(i2c0)
#define LED4_NODE DT_NODELABEL(whiteled)

static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
static const struct gpio_dt_spec whiteled_spec = GPIO_DT_SPEC_GET(LED4_NODE, gpios);

int cls16d24_init(void)
{
    printk("cls16d24 init\n");
    return 0;
}

static int readreg(uint8_t reg, uint8_t *val)
{
    return i2c_write_read(i2c_dev, CLS_16D24_44_DF8_TR8, &reg, 1, val, 1);
}

static int writereg(uint8_t reg, uint8_t val)
{
    return i2c_reg_write_byte(i2c_dev, CLS_16D24_44_DF8_TR8, reg, val);
}

int cls16d24_ledenable(void)
{
    gpio_pin_configure_dt(&whiteled_spec, GPIO_OUTPUT);
    gpio_pin_set_dt(&whiteled_spec, 1);
    printk("led HIGH \n");
    k_msleep(1000);
    return 0;
}

int cls16d24_sysmctrl(unit8_t val)
{
    write(SYSM_CTRL, val);
    return 0;
}

int cls16d24_intflag(unit8_t val)
{
    write(INT_FLAG, val);
    return 0;
}

int cls16d24_waittime(unit8_t val)
{
    write(WAIT_TIME, val);
    return 0;
}

int cls16d24_clstime(unit8_t val)
{
    write(CLS_TIME, val);
    return 0;
}

int cls16d24_rgb(void)
{
    uint8_t x, val0, val1, val2, val3, val4, val5, val6, val7;
    read(RCH_DATA_H, &val0);
    read(RCH_DATA_L, &val1);
    read(GCH_DATA_H, &val2);
    read(GCH_DATA_L, &val3);
    read(BCH_DATA_H, &val4);
    read(BCH_DATA_L, &val5);
    read(WCH_DATA_H, &val6);
    read(WCH_DATA_L, &val7);

    int16_t red = ((int16_t)val0 << 8) | val1;

    int16_t green = ((int16_t)val2 << 8) | val3;

    int16_t blue = ((int16_t)val4 << 4) | val5;

    int16_t white = ((int16_t)val6 << 8) | val7;

    k_msleep(1000);
    return 0;
}

int cls16d24_whoami(void)
{
    ret = read(PROD_ID_L, &x);
    if (ret == 0)
    {
        printk("WHOAMI: 0x%02X\n", x);
    }
    else
    {
        printk("I2C read failed: %d\n", ret);
    }
    return 0;
}
