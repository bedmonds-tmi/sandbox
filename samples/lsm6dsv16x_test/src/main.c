#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

void main(void)
{
	/* Find the sensor automatically based on the compatible string in the overlay */
	const struct device *const dev = DEVICE_DT_GET_ANY(st_lsm6dsv16x);

	if (dev == NULL) {
		printf("Error: No LSM6DSV16X device found in devicetree\n");
		return;
	}
	if (!device_is_ready(dev)) {
		printf("Error: Device %s is not ready\n", dev->name);
		return;
	}

	printf("Found device: %s\n", dev->name);

	/* Set sampling frequency to a low rate: 15 Hz */
	struct sensor_value odr_attr = {.val1 = 7680, .val2 = 0};

	if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) <
	    0) {
		printf("Warning: Cannot configure accelerometer ODR\n");
	}

	if (sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) <
	    0) {
		printf("Warning: Cannot configure gyroscope ODR\n");
	}

	struct sensor_value accel[3];
	struct sensor_value gyro[3];

	/* Give the serial connection a brief moment to settle, then send the configuration header
	 */
	k_sleep(K_MSEC(500));
	printf("header X:'#e74c3c' Y:'#3498db' Z:'#2ecc71' Xg:'#f39c12' Yg:'#9b59b6' "
	       "Zg:'#1abc9c'\n");

	while (1) {
		/* Tell the driver to read the latest data from the sensor over SPI */
		if (sensor_sample_fetch(dev) < 0) {
			printf("Error: Failed to fetch sensor sample\n");
			return;
		}

		/* Extract the fetched data into our arrays */
		sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel);
		sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro);

		/* Print out named data points recognized automatically by Muino Serial Plotter */
		printf("X:%f Y:%f Z:%f Xg:%f Yg:%f Zg:%f\n", sensor_value_to_double(&accel[0]),
		       sensor_value_to_double(&accel[1]), sensor_value_to_double(&accel[2]),
		       sensor_value_to_double(&gyro[0]), sensor_value_to_double(&gyro[1]),
		       sensor_value_to_double(&gyro[2]));
	}
}
