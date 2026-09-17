/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * LSM6DSV16X over SPI at 7680Hz, streaming raw accelerometer samples to the
 * host as framed binary for spectral analysis -- built to characterise the
 * mechanical transient of an inhaler cartridge actuation.
 *
 * Deliberately raw. No filtering happens here: an actuation is a broadband
 * impulse and the whole point is to find which bands carry it, so anything
 * shaped on-device would be shaping the evidence. Nyquist is 3840Hz.
 *
 * Batches come from the hardware FIFO, 255 samples per watermark interrupt.
 * See prj.conf for why streaming needs TRIGGER_NONE.
 *
 * Wire format, little-endian throughout (both ends are LE):
 *
 *   magic  u32   0x5AA55AA5
 *   seq    u32   batch counter; gaps tell the host it dropped a frame
 *   n      u16   samples in this frame
 *   shift  i8    scale exponent: m/s^2 = sample * 2^shift / 32768
 *   axes   u8    3
 *   data   i16[n][3]   x, y, z
 *   sum    u16   sum of every data byte, truncated
 *
 * The host resynchronises on the magic and drops frames failing the checksum,
 * so a log line landing mid-frame costs one batch rather than the stream.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(imu_spi_stream, LOG_LEVEL_INF);

#define IMU_NODE     DT_NODELABEL(imu)
#define DECODE_CHUNK 32
#define FRAME_MAGIC  0x5AA55AA5
#define MAX_SAMPLES  256
#define AXES         3

SENSOR_DT_STREAM_IODEV(imu_fifo, IMU_NODE,
		       {SENSOR_TRIG_FIFO_WATERMARK, SENSOR_STREAM_DATA_INCLUDE});

RTIO_DEFINE_WITH_MEMPOOL(imu_rtio, 4, 4, 64, 256, sizeof(void *));

static const struct device *const uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static const struct sensor_chan_spec accel_chan = {SENSOR_CHAN_ACCEL_XYZ, 0};

/* Union with the struct so the decoder's uint64_t timestamp lands aligned. */
static union {
	struct sensor_three_axis_data data;
	uint8_t bytes[sizeof(struct sensor_three_axis_data) +
		      (DECODE_CHUNK - 1) * sizeof(struct sensor_three_axis_sample_data)];
} chunk;

struct frame_hdr {
	uint32_t magic;
	uint32_t seq;
	uint16_t n;
	int8_t shift;
	uint8_t axes;
} __packed;

static int16_t payload[MAX_SAMPLES * AXES];

static void emit(const void *p, size_t len)
{
	const uint8_t *b = p;

	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart, b[i]);
	}
}

static void send_frame(uint32_t seq, uint16_t n, int8_t shift)
{
	struct frame_hdr hdr = {
		.magic = FRAME_MAGIC,
		.seq = seq,
		.n = n,
		.shift = shift,
		.axes = AXES,
	};
	size_t bytes = (size_t)n * AXES * sizeof(int16_t);
	const uint8_t *b = (const uint8_t *)payload;
	uint16_t sum = 0;

	for (size_t i = 0; i < bytes; i++) {
		sum += b[i];
	}

	emit(&hdr, sizeof(hdr));
	emit(payload, bytes);
	emit(&sum, sizeof(sum));
}

/*
 * The decoder hands back q31 with a shift; the top 16 bits keep the part's own
 * resolution (2^shift/32768 = 7.8mm/s^2 at the 16g range, against its 4.8mm/s^2
 * LSB) at half the bandwidth of sending q31 whole, which would not fit.
 */
static uint16_t decode_batch(const struct sensor_decoder_api *dec, uint8_t *buf, int8_t *shift)
{
	uint32_t fit = 0;
	uint16_t n = 0;
	int got;

	while ((got = dec->decode(buf, accel_chan, &fit, DECODE_CHUNK, &chunk.data)) > 0) {
		for (int i = 0; i < got && n < MAX_SAMPLES; i++, n++) {
			payload[n * AXES + 0] = (int16_t)(chunk.data.readings[i].x >> 16);
			payload[n * AXES + 1] = (int16_t)(chunk.data.readings[i].y >> 16);
			payload[n * AXES + 2] = (int16_t)(chunk.data.readings[i].z >> 16);
		}
		*shift = chunk.data.shift;
	}

	return n;
}

int main(void)
{
	const struct device *dev = DEVICE_DT_GET(IMU_NODE);
	const struct sensor_decoder_api *dec;
	struct rtio_sqe *handle;
	uint32_t seq = 0;
	int rc;

	if (!device_is_ready(dev)) {
		LOG_ERR("%s not ready -- check CS reaches P1.11 and is not strapped to VDD",
			dev->name);
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

	/* Last line before the UART carries binary only. */
	LOG_INF("streaming binary frames");
	k_sleep(K_MSEC(50));

	while (true) {
		struct rtio_cqe *cqe = rtio_cqe_consume_block(&imu_rtio);
		uint8_t *buf;
		uint32_t buf_len;
		int8_t shift = 0;
		uint16_t n;

		if (cqe->result != 0) {
			rtio_cqe_release(&imu_rtio, cqe);
			continue;
		}

		rc = rtio_cqe_get_mempool_buffer(&imu_rtio, cqe, &buf, &buf_len);
		rtio_cqe_release(&imu_rtio, cqe);
		if (rc != 0) {
			continue;
		}

		n = decode_batch(dec, buf, &shift);
		rtio_release_buffer(&imu_rtio, buf, buf_len);

		if (n > 0) {
			send_frame(seq++, n, shift);
		}
	}

	return 0;
}
