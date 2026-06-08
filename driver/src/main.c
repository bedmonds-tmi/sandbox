#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#define I2C_NODE DT_NODELABEL(i2c0)
#define MPU6050_ACCEL_XOUT 0x3C
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_REG_WHOAMI 0x75
#define MPU6050_REG_USER_CTRL 0x6A

const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

int mpu6050_read(uint8_t reg, uint8_t *val)
{
  return i2c_write_read(i2c_dev, MPU6050_I2C_ADDR, &reg, 1, val, 1);
}

int mpu6050_write(uint8_t reg, uint8_t val)
{
  return i2c_reg_write_byte(i2c_dev, MPU6050_I2C_ADDR, reg, val);
}

/** True enables fifo */
int mpu6050_fifo_enable(bool enable)
{
  uint8_t tmp;

  int ret = mpu6050_read(MPU6050_REG_USER_CTRL, &tmp);
  if (ret != 0)
  {
    printk("Error: %d\n", ret);
  }

  if (enable)
  {
    tmp |= (1 << 6);
  }
  else
  {
    tmp &= ~(1 << 6);
  }
  printk("Writing FIFO_EN: 0x%02X\n", tmp);
  return mpu6050_write(MPU6050_REG_USER_CTRL, tmp);
}

int main(void)
{
  int ret;

  if (!device_is_ready(i2c_dev))
  {
    printk("I2C device not ready\n");
    return 0;
  }

  uint8_t x;
  ret = mpu6050_read(MPU6050_REG_WHOAMI, &x);
  if (ret == 0)
  {
    printk("WHOAMI: 0x%02X\n", (x >> 1) & 0x3F);
  }
  else
  {
    printk("I2C read failed: %d\n", ret);
  }

  mpu6050_write(0x6B, 0);

  while (1)
  {
    uint8_t val0, val1;

    mpu6050_read(0x3B, &val0);
    mpu6050_read(0x3C, &val1);

    int16_t val = ((int16_t)val0 << 8) | val1;
    printk(">ax:%d\n", val);

    k_msleep(50);
  }
}