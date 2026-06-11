#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#define I2C_NODE DT_NODELABEL(i2c0)

#define MPU6050_ACCEL_XOUT 0x3C
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_REG_WHOAMI 0x75
#define MPU6050_REG_USER_CTRL 0x6A
#define MPU6050_SELF_TEST_X 0x0D
#define MPU6050_SELF_TEST_Y 0x0E
#define MPU6050_SELF_TEST_Z 0x0F
#define MPU6050_SELF_TEST_A 0x10
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_SMPLRT_DIV 0x19
#define MPU6050_CONFIG 0x1A
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_MOT_THR 0x1F
#define MPU6050_FIFO_EN 0x23
#define MPU6050_I2C_MST_CTRL
#define MPU6050_I2C_SLV0_ADDR
#define MPU6050_I2C_SLV0_REG
#define MPU6050_I2C_SLV0_CTRL
#define MPU6050_I2C_SLV1_ADDR
#define MPU6050_I2C_SLV1_REG
#define MPU6050_I2C_SLV1_CTRL
#define MPU6050_I2C_SLV2_ADDR
#define MPU6050_I2C_SLV2_REG
#define MPU6050_I2C_SLV2_CTRL
#define MPU6050_I2C_SLV3_ADDR
#define MPU6050_I2C_SLV3_REG
#define MPU6050_I2C_SLV3_CTRL
#define MPU6050_I2C_SLV4_ADDR
#define MPU6050_I2C_SLV4_REG
#define MPU6050_I2C_SLV4_DO
#define MPU6050_I2C_SLV4_CTRL
#define MPU6050_REG_WHOAMI 0x75
#define MPU6050_REG_USER_CTRL 0x6A

long temp_calibration_value = 0;
long gyroX_offset = 0;
long gyroY_offset = 0;
long gyroZ_offset = 0;
int gyroScale = 131; // For ±250 °/s

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

void track_x_axis(bool enable)
{
    uint8_t val0, val1;

    mpu6050_read(0x3B, &val0);
    mpu6050_read(0x3C, &val1);

    int16_t val = ((int16_t)val0 << 8) | val1;

    printk(">ax:%d\n", val);
}

void track_y_axis(bool enable)
{
    uint8_t val0, val1;

    mpu6050_read(0x3D, &val0);
    mpu6050_read(0x3E, &val1);

    int16_t val = ((int16_t)val0 << 8) | val1;
    printk(">ay:%d\n", val);
}

void track_z_axis(bool enable)
{
    uint8_t val0, val1;

    mpu6050_read(0x3F, &val0);
    mpu6050_read(0x40, &val1);

    int16_t val = ((int16_t)val0 << 8) | val1;
    printk(">az:%d\n", val);
}

void track_gyro_x(bool enable)
{
    uint8_t val0, val1;

    mpu6050_read(0x43, &val0);
    mpu6050_read(0x44, &val1);

    int16_t val = ((int16_t)val0 << 8) | val1;
    printk(">gx:%ld\n", (val - gyroX_offset) / gyroScale);
}

void track_gyro_y(bool enable)
{
    uint8_t val0, val1;

    mpu6050_read(0x45, &val0);
    mpu6050_read(0x46, &val1);

    int16_t val = ((int16_t)val0 << 8) | val1;
    printk(">gy:%ld\n", (val - gyroY_offset) / gyroScale);
}

void track_gyro_z(bool enable)
{
    uint8_t val0, val1;

    mpu6050_read(0x47, &val0);
    mpu6050_read(0x48, &val1);

    int16_t val = ((int16_t)val0 << 8) | val1;
    printk(">gz:%ld\n", (val - gyroZ_offset) / gyroScale);
}

void calibrate_temperature(void)
{
    long sumT = 0;
    for (int i = 0; i < 10000; i++)
    {
        uint8_t val0, val1;

        mpu6050_read(0x41, &val0);
        mpu6050_read(0x42, &val1);

        int16_t val = ((int16_t)val0 << 8) | val1;
        sumT += val;
    }
    long avgT = sumT / 10000;
    temp_calibration_value = avgT;
    printk("temp calibration value: %ld\n", avgT);
}

void track_temp(bool enable)
{
    uint8_t val0, val1;
    mpu6050_read(0x41, &val0);
    mpu6050_read(0x42, &val1);
    int16_t val = ((int16_t)val0 << 8) | val1;
    long rt = val / 340;
    rt = rt + 36.53;
    printk(">temp:%ld \n", rt);
}

void calibrate_gyroscope(void)
{
    long sumX = 0;
    printk("Calibrating Gyroscope... Keep the sensor still.");
    for (int i = 0; i < 200; i++)
    {
        uint8_t val0, val1;
        mpu6050_read(0x43, &val0);
        mpu6050_read(0x44, &val1);
        int16_t gyroX = ((int16_t)val0 << 8) | val1;
        sumX += gyroX;
        mpu6050_read(0x45, &val0);
        mpu6050_read(0x46, &val1);
        int16_t gyroY = ((int16_t)val0 << 8) | val1;
        sumX += gyroY;
        mpu6050_read(0x47, &val0);
        mpu6050_read(0x48, &val1);
        int16_t gyroZ = ((int16_t)val0 << 8) | val1;
        sumX += gyroZ;
    }
    long avgX = sumX / 200;
    long avgY = sumX / 200;
    long avgZ = sumX / 200;
    gyroX_offset = avgX;
    gyroY_offset = avgY;
    gyroZ_offset = avgZ;
}

void device_whoami(void)
{
    int ret;
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
}

int main(void)
{

    if (!device_is_ready(i2c_dev))
    {
        printk("I2C device not ready\n");
        return -1;
    }
    device_whoami();

    mpu6050_write(0x6B, 0);

    while (1)
    {
        track_temp(true);
        k_msleep(1000);
    }
}
