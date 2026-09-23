"""
tmag5273_read.py
=================
Single-file script: reads the TMAG5273 Hall-effect sensor over I2C
(via an MCP2221 USB adapter) and prints X/Y/Z magnetic field and
temperature to the terminal. No GUI, no GPIO/button - just I2C polling.

Wiring:
    TMAG5273 VCC  -> 1.7-3.6V supply
    TMAG5273 GND  -> GND
    TMAG5273 SCL  -> MCP2221 SCL
    TMAG5273 SDA  -> MCP2221 SDA
    TMAG5273 TEST -> GND

Install dependency:
    pip install easymcp2221
Run:
    python3 tmag5273_read.py
"""

import time
import sys

import EasyMCP2221
from EasyMCP2221.exceptions import NotAckError, LowSCLError, LowSDAError

# ---------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------
I2C_ADDR = 0x22       # default factory TMAG5273 7-bit I2C address
RANGE_MT = 40          # TMAG5273A1, _RANGE=0b -> +/-40mT (see datasheet Table 6-3)
POLL_INTERVAL_S = 0.1

# Register offsets
REG_DEVICE_CONFIG_1 = 0x00
REG_DEVICE_CONFIG_2 = 0x01
REG_SENSOR_CONFIG_1 = 0x02
REG_SENSOR_CONFIG_2 = 0x03
REG_T_CONFIG = 0x07
REG_DEVICE_ID = 0x0D
REG_T_MSB_RESULT = 0x10  # T_MSB..Z_LSB are sequential (0x10-0x17)

OPERATING_MODE_CONTINUOUS = 0b10
MAG_CH_EN_XYZ = 0b0111

T_SENS_T0_C = 25
T_ADC_T0 = 17508
T_ADC_RES = 58.0


def i2c_write(dev, addr, data_bytes):
    try:
        dev.I2C_write(addr, bytes(data_bytes))
        return True
    except (NotAckError, LowSCLError, LowSDAError) as e:
        print(f"WARNING: I2C write error: {e}")
        return False


def i2c_read(dev, addr, n):
    try:
        return list(dev.I2C_read(addr, n))
    except (NotAckError, LowSCLError, LowSDAError) as e:
        print(f"WARNING: I2C read error: {e}")
        return None


def i2c_write_then_read(dev, addr, write_bytes, n):
    if not i2c_write(dev, addr, write_bytes):
        return None
    return i2c_read(dev, addr, n)


def to_signed16(msb, lsb):
    raw = (msb << 8) | lsb
    return raw - 0x10000 if raw >= 0x8000 else raw


def raw_to_mT(raw_signed):
    return raw_signed / 32768.0 * RANGE_MT


def raw_to_temp_c(raw_signed):
    return T_SENS_T0_C + (raw_signed - T_ADC_T0) / T_ADC_RES


def main():
    try:
        dev = EasyMCP2221.Device()
    except Exception as e:
        print(f"ERROR: could not open MCP2221 device: {e}")
        sys.exit(1)

    # Sanity check: read DEVICE_ID
    result = i2c_write_then_read(dev, I2C_ADDR, [REG_DEVICE_ID], 1)
    if result is None:
        print("ERROR: no response from TMAG5273. Check wiring/address.")
        sys.exit(1)
    print(f"TMAG5273 found (DEVICE_ID = 0x{result[0]:02X}). Starting continuous read...\n")

    # Configure: enable X, Y, Z channels
    i2c_write(dev, I2C_ADDR, [REG_SENSOR_CONFIG_1, MAG_CH_EN_XYZ << 4])
    # Keep default (narrow) magnetic range
    i2c_write(dev, I2C_ADDR, [REG_SENSOR_CONFIG_2, 0x00])
    # Enable temperature channel
    i2c_write(dev, I2C_ADDR, [REG_T_CONFIG, 0b1])
    # No CRC, no averaging, standard read mode
    i2c_write(dev, I2C_ADDR, [REG_DEVICE_CONFIG_1, 0x00])
    # Continuous measure mode
    i2c_write(dev, I2C_ADDR, [REG_DEVICE_CONFIG_2, OPERATING_MODE_CONTINUOUS])

    time.sleep(0.01)

    print(f"{'Temp (C)':>10} {'X (mT)':>10} {'Y (mT)':>10} {'Z (mT)':>10}")
    print("-" * 44)

    try:
        while True:
            data = i2c_write_then_read(dev, I2C_ADDR, [REG_T_MSB_RESULT], 8)
            if data is None or len(data) < 8:
                print("  (I2C read error, retrying...)")
            else:
                t = raw_to_temp_c(to_signed16(data[0], data[1]))
                x = raw_to_mT(to_signed16(data[2], data[3]))
                y = raw_to_mT(to_signed16(data[4], data[5]))
                z = raw_to_mT(to_signed16(data[6], data[7]))
                print(f"{t:10.2f} {x:10.3f} {y:10.3f} {z:10.3f}")
            time.sleep(POLL_INTERVAL_S)
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
