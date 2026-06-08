#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

#define I2C_NODE DT_NODELABEL(i2c0)
#define CLS_16D24_44_DF8_TR8 0x38
#define SYSM_CTRL 0x00
#define INT_FLAG 0x02
#define WAIT_TIME 0x03
#define CLS_GAIN 0x04
#define CLS_TIME 0x05
#define PROD_ID_L 0xBC
#define PROD_ID_H 0xBD
#define ERROR_FLAG 0x17
#define RCH_DATA_L 0x1C
#define RCH_DATA_H 0x1D
#define GCH_DATA_L 0x1E
#define GCH_DATA_H 0x1F
#define BCH_DATA_L 0x20
#define BCH_DATA_H 0x21
#define WCH_DATA_L 0x22
#define WCH_DATA_H 0x23
#define IRCH_DATA_L 0x24
#define IRCH_DATA_H 0x25
#define COL_REG_COUNT 10U
#define ID_BYTE_COUNT 2U

#define LED4_NODE DT_NODELABEL(whiteled)

#define MY_GPIO_NODE DT_NODELABEL(my_gpio)
static const struct gpio_dt_spec my_pin = GPIO_DT_SPEC_GET(MY_GPIO_NODE, gpios);

const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

int cls16d24_read(uint8_t reg, uint8_t *val)
{
    return i2c_write_read(i2c_dev, CLS_16D24_44_DF8_TR8, &reg, 1, val, 1);
}

int cls16d24_write(uint8_t reg, uint8_t val)
{
    return i2c_reg_write_byte(i2c_dev, CLS_16D24_44_DF8_TR8, reg, val);
}

static const struct gpio_dt_spec whiteled_spec = GPIO_DT_SPEC_GET(LED4_NODE, gpios);

int main(void)
{
    gpio_pin_configure_dt(&whiteled_spec, GPIO_OUTPUT);

    gpio_pin_set_dt(&whiteled_spec, 1);
    printk("led HIGH \n");
    k_msleep(2000);

    cls16d24_write(SYSM_CTRL, 1);
    cls16d24_write(INT_FLAG, 0);
    cls16d24_write(WAIT_TIME, 0);
    cls16d24_write(CLS_GAIN, 136);
    cls16d24_write(CLS_TIME, 50);
    // ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    while (1)
    {
        uint8_t x, val0, val1, val2, val3, val4, val5, val6, val7;
        cls16d24_read(RCH_DATA_H, &val0);
        cls16d24_read(RCH_DATA_L, &val1);
        cls16d24_read(GCH_DATA_H, &val2);
        cls16d24_read(GCH_DATA_L, &val3);
        cls16d24_read(BCH_DATA_H, &val4);
        cls16d24_read(BCH_DATA_L, &val5);
        cls16d24_read(WCH_DATA_H, &val6);
        cls16d24_read(WCH_DATA_L, &val7);

        int16_t red = ((int16_t)val0 << 8) | val1;
        printk(">red:%d\n", red);
        int16_t green = ((int16_t)val2 << 8) | val3;
        printk(">green:%d\n", green);
        int16_t blue = ((int16_t)val4 << 4) | val5;
        printk(">blue:%d\n", blue);
        int16_t white = ((int16_t)val6 << 8) | val7;
        printk(">white:%d\n", white);
        k_msleep(2000);
    }
}