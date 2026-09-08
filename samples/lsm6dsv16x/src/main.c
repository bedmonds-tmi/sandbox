/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Batch processing from the LSM6DSV16X FIFO. The accelerometer is batched at
 * its ODR and the watermark is maxed, so one interrupt delivers 255 samples.
 * Each batch is decoded, low-pass filtered, and printed as raw vs filtered.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/rtio/rtio.h>

LOG_MODULE_REGISTER(imu_batch, LOG_LEVEL_INF);

#define IMU_NODE     DT_NODELABEL(imu)
#define PRINT_EVERY  16 /* the UART cannot carry 3840 lines/s */
#define DECODE_CHUNK 32

/* Cutoff = accel ODR / (2^LPF_SHIFT * 2pi) ~= 4.8Hz at 3840Hz. Retune with the ODR. */
#define LPF_SHIFT 7
/* Extra fractional bits, or the shift truncates to zero and the filter stalls. */
#define LPF_FRAC  8

SENSOR_DT_STREAM_IODEV(imu_fifo, IMU_NODE,
		       {SENSOR_TRIG_FIFO_WATERMARK, SENSOR_STREAM_DATA_INCLUDE});

RTIO_DEFINE_WITH_MEMPOOL(imu_rtio, 4, 4, 64, 256, sizeof(void *));

static const struct sensor_chan_spec accel_chan = {SENSOR_CHAN_ACCEL_XYZ, 0};

/* Union with the struct so the decoder's uint64_t timestamp lands aligned. */
static union {
	struct sensor_three_axis_data data;
	uint8_t bytes[sizeof(struct sensor_three_axis_data) +
		      (DECODE_CHUNK - 1) * sizeof(struct sensor_three_axis_sample_data)];
} chunk;

static int32_t q31_to_milli(q31_t q, int8_t shift)
{
	return (int32_t)(((int64_t)q * 1000) >> (31 - shift));
}

static void low_pass(const int32_t in[3], int32_t out[3])
{
	static int32_t state[3];
	static bool primed;

	for (int i = 0; i < 3; i++) {
		if (primed) {
			state[i] += ((in[i] << LPF_FRAC) - state[i]) >> LPF_SHIFT;
		} else {
			state[i] = in[i] << LPF_FRAC;
		}
		out[i] = state[i] >> LPF_FRAC;
	}

	primed = true;
}

#define MILLI_FMT "%s%d.%03d"
#define MILLI_ARG(m)                                                                               \
	((m) < 0 ? "-" : ""), (int)(((m) < 0 ? -(m) : (m)) / 1000),                                \
		(int)(((m) < 0 ? -(m) : (m)) % 1000)

static void print_sample(const int32_t raw[3], const int32_t lpf[3], uint16_t batch)
{
	LOG_PRINTK("raw_x:" MILLI_FMT ",raw_y:" MILLI_FMT ",raw_z:" MILLI_FMT ",lpf_x:" MILLI_FMT
		   ",lpf_y:" MILLI_FMT ",lpf_z:" MILLI_FMT ",n:%u\n",
		   MILLI_ARG(raw[0]), MILLI_ARG(raw[1]), MILLI_ARG(raw[2]), MILLI_ARG(lpf[0]),
		   MILLI_ARG(lpf[1]), MILLI_ARG(lpf[2]), batch);
}

static void process_batch(const struct sensor_decoder_api *dec, uint8_t *buf)
{
	static uint32_t seq;
	uint16_t batch = 0;
	uint32_t fit = 0;
	int n;

	if (dec->get_frame_count(buf, accel_chan, &batch) != 0) {
		return;
	}

	while ((n = dec->decode(buf, accel_chan, &fit, DECODE_CHUNK, &chunk.data)) > 0) {
		for (int i = 0; i < n; i++) {
			int32_t raw[3] = {
				q31_to_milli(chunk.data.readings[i].x, chunk.data.shift),
				q31_to_milli(chunk.data.readings[i].y, chunk.data.shift),
				q31_to_milli(chunk.data.readings[i].z, chunk.data.shift),
			};
			int32_t lpf[3];

			low_pass(raw, lpf);

			if (++seq % PRINT_EVERY == 0) {
				print_sample(raw, lpf, batch);
			}
		}
	}
}

int main(void)
{
	const struct device *dev = DEVICE_DT_GET(IMU_NODE);
	const struct sensor_decoder_api *dec;
	struct rtio_sqe *handle;
	int rc;

	if (!device_is_ready(dev)) {
		LOG_ERR("%s not ready", dev->name);
		return 0;
	}

	rc = sensor_get_decoder(dev, &dec);
	if (rc != 0) {
		LOG_ERR("sensor_get_decoder: %d", rc);
		return 0;
	}

	rc = sensor_stream(&imu_fifo, &imu_rtio, NULL, &handle);
	if (rc != 0) {
		LOG_ERR("sensor_stream: %d", rc);
		return 0;
	}

	while (true) {
		struct rtio_cqe *cqe = rtio_cqe_consume_block(&imu_rtio);
		uint8_t *buf;
		uint32_t buf_len;

		if (cqe->result != 0) {
			LOG_ERR("stream read: %d", cqe->result);
			rtio_cqe_release(&imu_rtio, cqe);
			continue;
		}

		rc = rtio_cqe_get_mempool_buffer(&imu_rtio, cqe, &buf, &buf_len);
		rtio_cqe_release(&imu_rtio, cqe);
		if (rc != 0) {
			continue;
		}

		process_batch(dec, buf);
		rtio_release_buffer(&imu_rtio, buf, buf_len);
	}

	return 0;
}
