#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tmag_test, LOG_LEVEL_INF);

/*
 * This looks up the 'tmag5273' node label we defined in the
 * devicetree overlay and extracts the bus controller and address.
 */
#define TMAG5273_NODE DT_NODELABEL(d1)
const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(TMAG5273_NODE);

#if !DT_NODE_EXISTS(TMAG5273_NODE)
#error "TMAG5273 devicetree node not found! Check your overlay."
#endif

/* TMAG5273 Register Offsets */
#define TMAG5273_REG_DEVICE_CONFIG_2 0x01
#define TMAG5273_REG_SENSOR_CONFIG_1 0x02
#define TMAG5273_REG_DEVICE_ID       0x0D
#define TMAG5273_REG_X_MSB_RESULT    0x12

/* Configuration Bits */
#define TMAG5273_OPERATING_MODE_CONTINUOUS (2 << 0)
#define TMAG5273_MAG_CH_EN_XYZ             (0x7 << 4) // Enable X, Y, and Z channels

/* Moving Average Filter Settings */
#define MA_WINDOW_SIZE 8

int main(void)
{
	uint8_t device_id = 0;
	uint8_t xyz_data[6];
	int16_t x_raw, y_raw, z_raw;
	int16_t x_filtered, y_filtered, z_filtered;
	int ret;

	/* Filter buffers and state tracking */
	int16_t x_buf[MA_WINDOW_SIZE] = {0};
	int16_t y_buf[MA_WINDOW_SIZE] = {0};
	int16_t z_buf[MA_WINDOW_SIZE] = {0};
	uint8_t head = 0;
	uint8_t current_size = 0;
	int32_t x_sum = 0, y_sum = 0, z_sum = 0;

	LOG_INF("Starting TMAG5273 XYZ Sensor reader test with Moving Average Filter...");

	/* Check if the underlying I2C controller/bus is ready */
	if (!device_is_ready(dev_i2c.bus)) {
		LOG_ERR("I2C bus %s is not ready!", dev_i2c.bus->name);
		return 0;
	}

	LOG_INF("I2C bus ready. Verifying Device ID at address 0x%02X...", dev_i2c.addr);

	/* Read Device ID */
	ret = i2c_reg_read_byte_dt(&dev_i2c, TMAG5273_REG_DEVICE_ID, &device_id);
	if (ret < 0) {
		LOG_ERR("Failed to read Device ID (err %d)", ret);
	} else {
		LOG_INF("TMAG5273 Device ID: 0x%02X", device_id);
	}

	/* Configure sensor: Enable X, Y, Z channels */
	ret = i2c_reg_write_byte_dt(&dev_i2c, TMAG5273_REG_SENSOR_CONFIG_1, TMAG5273_MAG_CH_EN_XYZ);
	if (ret < 0) {
		LOG_ERR("Failed to configure sensor channels (err %d)", ret);
		return 0;
	}

	/* Configure sensor: Set operating mode to Continuous conversion */
	ret = i2c_reg_write_byte_dt(&dev_i2c, TMAG5273_REG_DEVICE_CONFIG_2,
				    TMAG5273_OPERATING_MODE_CONTINUOUS);
	if (ret < 0) {
		LOG_ERR("Failed to configure operating mode (err %d)", ret);
		return 0;
	}

	LOG_INF("TMAG5273 configured successfully for continuous XYZ reading.");

	while (1) {
		/*
		 * Burst read 6 bytes starting from X MSB (0x12):
		 * [0]: X_MSB, [1]: X_LSB, [2]: Y_MSB, [3]: Y_LSB, [4]: Z_MSB, [5]: Z_LSB
		 */
		ret = i2c_burst_read_dt(&dev_i2c, TMAG5273_REG_X_MSB_RESULT, xyz_data,
					sizeof(xyz_data));

		if (ret < 0) {
			LOG_ERR("Failed to read XYZ data from TMAG5273 sensor (err %d)", ret);
		} else {
			/* Combine MSB and LSB into 16-bit signed raw values */
			x_raw = (int16_t)((xyz_data[0] << 8) | xyz_data[1]);
			y_raw = (int16_t)((xyz_data[2] << 8) | xyz_data[3]);
			z_raw = (int16_t)((xyz_data[4] << 8) | xyz_data[5]);

			/* --- Moving Average Filter Logic --- */

			// Subtract the oldest sample from the running sum if buffer is full
			if (current_size == MA_WINDOW_SIZE) {
				x_sum -= x_buf[head];
				y_sum -= y_buf[head];
				z_sum -= z_buf[head];
			} else {
				current_size++;
			}

			// Insert new raw reading into the buffer
			x_buf[head] = x_raw;
			y_buf[head] = y_raw;
			z_buf[head] = z_raw;

			// Add the new sample to the running sum
			x_sum += x_raw;
			y_sum += y_raw;
			z_sum += z_raw;

			// Advance the circular buffer head pointer
			head = (head + 1) % MA_WINDOW_SIZE;

			// Calculate final averages
			x_filtered = (int16_t)(x_sum / current_size);
			y_filtered = (int16_t)(y_sum / current_size);
			z_filtered = (int16_t)(z_sum / current_size);

			/* Output raw vs filtered if you want to compare, or just print filtered */
			LOG_INF("Raw  -> X: %6d | Y: %6d | Z: %6d", x_raw, y_raw, z_raw);
			LOG_INF("Filt -> X: %6d | Y: %6d | Z: %6d", x_filtered, y_filtered,
				z_filtered);
		}

		k_msleep(500);
	}

	return 0;
}
