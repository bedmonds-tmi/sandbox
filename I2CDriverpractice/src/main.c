
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
// who am I will return 0xE0 for DRV2605L
#define DRV2605L_ADDR 0x5A
#define I2C_WHOAMI_REG 0x00
#define GAIN_REG 0x1A
#define CONFIG_REG 0x1D

// Device Active Mode Operation Registers
#define MODE_REG 0x01
#define RTP_INPUT 0x02
// these are not registers, they are values that cause different modes when written
#define DRV2605_REG_MODE 0x01         ///< Mode register
#define DRV2605_MODE_INTTRIG 0x00     ///< Internal trigger mode
#define DRV2605_MODE_EXTTRIGEDGE 0x01 ///< External edge trigger mode
#define DRV2605_MODE_EXTTRIGLVL 0x02  ///< External level trigger mode
#define DRV2605_MODE_PWMANALOG 0x03   ///< PWM/Analog input mode
#define DRV2605_MODE_AUDIOVIBE 0x04   ///< Audio-to-vibe mode
#define DRV2605_MODE_REALTIME 0x05    ///< Real-time playback (RTP) mode
#define DRV2605_MODE_DIAGNOS 0x06     ///< Diagnostics mode
#define DRV2605_MODE_AUTOCAL 0x07     ///< Auto calibration mode

#define DRV2605_REG_RTPIN 0x02    ///< Real-time playback input register
#define DRV2605_REG_LIBRARY 0x03  ///< Waveform library selection register
#define DRV2605_REG_WAVESEQ1 0x04 ///< Waveform sequence register 1
#define DRV2605_REG_WAVESEQ2 0x05 ///< Waveform sequence register 2
#define DRV2605_REG_WAVESEQ3 0x06 ///< Waveform sequence register 3
#define DRV2605_REG_WAVESEQ4 0x07 ///< Waveform sequence register 4
#define DRV2605_REG_WAVESEQ5 0x08 ///< Waveform sequence register 5
#define DRV2605_REG_WAVESEQ6 0x09 ///< Waveform sequence register 6
#define DRV2605_REG_WAVESEQ7 0x0A ///< Waveform sequence register 7
#define DRV2605_REG_WAVESEQ8 0x0B ///< Waveform sequence register 8

#define DRV2605_REG_GO 0x0C         ///< Go register
#define DRV2605_REG_OVERDRIVE 0x0D  ///< Overdrive time offset register
#define DRV2605_REG_SUSTAINPOS 0x0E ///< Sustain time offset, positive register
#define DRV2605_REG_SUSTAINNEG 0x0F ///< Sustain time offset, negative register
#define DRV2605_REG_BREAK 0x10      ///< Brake time offset register
#define DRV2605_REG_AUDIOCTRL 0x11  ///< Audio-to-vibe control register
#define DRV2605_REG_AUDIOLVL \
    0x12 ///< Audio-to-vibe minimum input level register
#define DRV2605_REG_AUDIOMAX \
    0x13 ///< Audio-to-vibe maximum input level register
#define DRV2605_REG_AUDIOOUTMIN \
    0x14 ///< Audio-to-vibe minimum output drive register
#define DRV2605_REG_AUDIOOUTMAX \
    0x15                        ///< Audio-to-vibe maximum output drive register
#define DRV2605_REG_RATEDV 0x16 ///< Rated voltage register
#define DRV2605_REG_CLAMPV 0x17 ///< Overdrive clamp voltage register
#define DRV2605_REG_AUTOCALCOMP \
    0x18 ///< Auto-calibration compensation result register
#define DRV2605_REG_AUTOCALEMP \
    0x19                          ///< Auto-calibration back-EMF result register
#define DRV2605_REG_FEEDBACK 0x1A ///< Feedback control register
#define DRV2605_REG_CONTROL1 0x1B ///< Control1 Register
#define DRV2605_REG_CONTROL2 0x1C ///< Control2 Register
#define DRV2605_REG_CONTROL3 0x1D ///< Control3 Register
#define DRV2605_REG_CONTROL4 0x1E ///< Control4 Register
#define DRV2605_REG_VBAT 0x21     ///< Vbat voltage-monitor register
#define DRV2605_REG_LRARESON 0x22 ///< LRA resonance-period register

#define I2C_NODE DT_NODELABEL(i2c0)

const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

int drv2605l_read(uint8_t reg, uint8_t *val)
{
    return i2c_write_read(i2c_dev, DRV2605L_ADDR, &reg, sizeof(reg), val, sizeof(*val));
}

int drv2605l_write(uint8_t reg, uint8_t val)
{
    return i2c_reg_write_byte(i2c_dev, DRV2605L_ADDR, reg, val);
}
int drv2605l_write_to_bit(uint8_t reg, uint8_t val, uint8_t bit_pos)
{
    uint8_t tmp;

    int ret = drv2605l_read(reg, &tmp);
    if (ret != 0)
    {
        printk("Error: %d\n", ret);
        return ret;
    }

    tmp &= ~(1 << bit_pos);                   // Clear the target bit
    tmp |= (val << bit_pos) & (1 << bit_pos); // Set the target bit based on val

    return drv2605l_write(reg, tmp);
}

int drv2605l_standby(bool enable) // set MODE_REG TO 0x00 from 0x40
{
    uint8_t tmp;

    int ret = drv2605l_read(MODE_REG, &tmp);
    if (ret != 0)
    {
        printk("Error: %d\n", ret);
    }

    if (enable)
    {
        tmp |= 0x40;
    }
    else
    {
        tmp &= ~0x40;
    }
    return drv2605l_write(MODE_REG, tmp);
}

int drv2605l_mode_to_autocalibration(bool enable) // set MODE_REG TO 0x00 from 0x40
{
    uint8_t tmp;

    int ret = drv2605l_read(MODE_REG, &tmp);
    if (ret != 0)
    {
        printk("Error: %d\n", ret);
    }

    if (enable)
    {
        tmp |= 0x07;
    }
    else
    {
        tmp &= ~0x07;
    }
    return drv2605l_write(MODE_REG, tmp);
}

void drv2605l_print_library_info(void)
{
    uint8_t lib_selection;
    int ret = drv2605l_read(DRV2605_REG_LIBRARY, &lib_selection);
    if (ret != 0)
    {
        printk("Error: %d\n", ret);
        return;
    }
    uint8_t lib_id = lib_selection & 0x1F; // Extract the library ID (bits 0-4)
    const char *lib_name;
    switch (lib_id)
    {
    case 0:
        lib_name = "Empty Library";
        break;
    case 1:
        lib_name = "A";
        break;
    case 2:
        lib_name = "B";
        break;
    case 3:
        lib_name = "C";
        break;
    case 4:
        lib_name = "D";
        break;
    case 5:
        lib_name = "E";
        break;
    case 6:
        lib_name = "LRA";
        break;
    case 7:
        lib_name = "F";
        break;
    }
    printk("Current Library: %s (ID: %d)\n", lib_name, lib_id);
}

void drv2605l_auto_config_routine_complete(void)
{ // check the 3 bit of the 0x00 register
    uint8_t tmp;
    int ret = drv2605l_read(I2C_WHOAMI_REG, &tmp);
    if (ret != 0)
    {
        printk("Error: %d\n", ret);
    }
    if ((tmp & 0x08) == 0) // Check if the 3rd bit is 0
    {
        printk("Auto-calibration complete and successful.\n");
    }
    else
    {
        printk("Auto-calibration failed or not complete.\n");
    }
}

int drv2605l_device_is_ready(void)
{
    int ret;

    if (!device_is_ready(i2c_dev))
    {
        printk("I2C device not ready\n");
        return 0;
    }

    uint8_t val;

    ret = drv2605l_read(I2C_WHOAMI_REG, &val);
    if (ret == 0)
    {
        printk("WHOAMI: 0x%02X\n", (val >> 5) & 0x07);
    }
    else
    {
        printk("I2C read failed: %d\n", ret);
    }
    return ret;
}

int main(void)
{
    uint8_t val;
    int ret = drv2605l_read(I2C_WHOAMI_REG, &val);
    if (ret == 0)
    {
        printk("WHOAMI: 0x%02X\n", (val >> 5) & 0x07);
    }
    else
    {
        printk("I2C read failed: %d\n", ret);
    }
    drv2605l_print_library_info();
    drv2605l_standby(true);
    drv2605l_read(MODE_REG, &val);
    printk("MODE_REG: 0x%02X\n", val);

    drv2605l_standby(false);
    drv2605l_mode_to_autocalibration(true);

    drv2605l_read(MODE_REG, &val);
    printk("MODE_REG post-auto-calibration: 0x%02X\n", val);

    drv2605l_read(DRV2605_REG_FEEDBACK, &val);
    printk("FEEDBACK_REG: 0x%02X\n", val);

    drv2605l_read(DRV2605_REG_RATEDV, &val);
    printk("Rated_VOLTAGE_REG: 0x%02X\n", val);

    drv2605l_write(DRV2605_REG_GO, 0x01);
    printk("Go register set to 0x01 to start the effect.\n");

    drv2605l_auto_config_routine_complete();
    k_msleep(1000); // Wait for a second to ensure the effect has time to play

    drv2605l_mode_to_autocalibration(false);
    drv2605l_read(MODE_REG, &val);
    printk("MODE_REG getting ready to recive internal go command: 0x%02X\n", val);

    drv2605l_write(DRV2605_REG_LIBRARY, 0x05);
    drv2605l_write(DRV2605_REG_WAVESEQ1, 0x01);
    drv2605l_write(DRV2605_REG_WAVESEQ2, 0x0E);

    drv2605l_read(DRV2605_REG_WAVESEQ1, &val);
    printk("WAVEFORM1_REG: 0x%02X\n", val);
    drv2605l_read(DRV2605_REG_WAVESEQ2, &val);
    printk("WAVEFORM2_REG: 0x%02X\n", val);

    drv2605l_write(DRV2605_REG_GO, 0x01);
    printk("Go register set to 0x01 to start the effect.\n");

    return 0;
}
