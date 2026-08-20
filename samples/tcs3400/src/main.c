/*
 * Copyright (c) 2026 Trudell Medical International
 * SPDX-License-Identifier: Apache-2.0
 *
 * TCS3400 RGBC colour sensor sample.
 *
 * Exercises the tmi,tcs3400 driver against a white illumination LED on a PWM
 * channel. The board overlay supplies both: a "color-sensor" alias for the
 * sensor node and a "white-led-pwm" alias for the LED.
 *
 * The LED ramps up and down continuously and samples stream out in teleplot
 * format (">name:value" per line) for the VS Code serial plotter, so the plot
 * sweeps the sensor's range against a known stimulus. One ramp step per
 * sample keeps the light level and the reading in lockstep. Sampling is
 * driven by the data-ready interrupt where the overlay wires an int-gpios
 * pin, and by polling otherwise.
 *
 * Gain, integration time and LED level can be retuned at runtime through the
 * "colour" shell command.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(tcs3400_sample);

/*
 * Brightness change per sample, as a percentage of full scale. 2% puts a
 * full up-and-down sweep in 100 samples, which is about 18 s at the
 * overlay's default 64 integration cycles.
 */
#define RAMP_STEP_PCT 2

/*
 * Longest a conversion can take: 256 integration cycles at 2.78 ms, plus
 * margin. Used as the interrupt wait timeout, so a missing or miswired INT
 * line shows up as a warning instead of a silent stall.
 */
#define SAMPLE_TIMEOUT_MS 1000

/* Sample interval when no data-ready interrupt is available. */
#define POLL_INTERVAL_MS 200

static const struct device *const sensor = DEVICE_DT_GET(DT_ALIAS(color_sensor));
static const struct pwm_dt_spec white_led = PWM_DT_SPEC_GET(DT_ALIAS(white_led_pwm));

static K_SEM_DEFINE(sample_ready, 0, 1);

/* Guards everything below against concurrent shell access. */
static K_MUTEX_DEFINE(colour_lock);
static struct sensor_value latest[4];
static bool sample_valid;
static bool streaming = true;
static bool ramping = true;
static uint8_t led_pct;
static bool ramp_rising = true;

/* Set once during startup; read by the sample loop to pick its wait strategy. */
static bool trigger_active;

static int white_led_set_pct(uint8_t pct)
{
	uint32_t pulse = (uint32_t)(((uint64_t)white_led.period * pct) / 100U);

	return pwm_set_pulse_dt(&white_led, pulse);
}

/*
 * Advance the fade one step, turning around at either end. Caller holds
 * colour_lock.
 */
static void ramp_advance(void)
{
	if (ramp_rising) {
		if (led_pct >= 100U - RAMP_STEP_PCT) {
			led_pct = 100U;
			ramp_rising = false;
		} else {
			led_pct += RAMP_STEP_PCT;
		}
	} else {
		if (led_pct <= RAMP_STEP_PCT) {
			led_pct = 0U;
			ramp_rising = true;
		} else {
			led_pct -= RAMP_STEP_PCT;
		}
	}

	int err = white_led_set_pct(led_pct);

	if (err) {
		LOG_ERR("failed to set white LED brightness (%d)", err);
	}
}

static void colour_data_ready(const struct device *dev, const struct sensor_trigger *trig)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trig);

	k_sem_give(&sample_ready);
}

/*
 * Register the data-ready trigger, falling back to polling rather than
 * failing: an overlay without the INT line wired still produces readings,
 * just on a timer.
 */
static void colour_trigger_init(void)
{
	static const struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};

	int err = sensor_trigger_set(sensor, &trig, colour_data_ready);

	if (err) {
		LOG_WRN("data-ready trigger unavailable (%d), polling every %d ms", err,
			POLL_INTERVAL_MS);
		return;
	}

	trigger_active = true;
}

static void wait_for_sample(void)
{
	if (!trigger_active) {
		k_msleep(POLL_INTERVAL_MS);
		return;
	}

	if (k_sem_take(&sample_ready, K_MSEC(SAMPLE_TIMEOUT_MS)) != 0) {
		LOG_WRN("no data-ready interrupt for %d ms, check the INT wiring",
			SAMPLE_TIMEOUT_MS);
	}
}

static void colour_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (!device_is_ready(sensor)) {
		LOG_ERR("colour sensor %s not ready", sensor->name);
		return;
	}

	if (!pwm_is_ready_dt(&white_led)) {
		LOG_ERR("white LED PWM %s not ready", white_led.dev->name);
		return;
	}

	if (white_led_set_pct(led_pct) != 0) {
		return;
	}

	colour_trigger_init();

	LOG_INF("TCS3400 ready, LED ramping, sampling on %s",
		trigger_active ? "the data-ready interrupt" : "a timer");

	while (1) {
		struct sensor_value crgb[4];

		wait_for_sample();

		int err = sensor_sample_fetch(sensor);

		if (err) {
			LOG_ERR("sample fetch failed (%d)", err);
			k_msleep(POLL_INTERVAL_MS);
			continue;
		}

		err = sensor_channel_get(sensor, SENSOR_CHAN_ALL, crgb);
		if (err) {
			LOG_ERR("channel get failed (%d)", err);
			k_msleep(POLL_INTERVAL_MS);
			continue;
		}

		k_mutex_lock(&colour_lock, K_FOREVER);

		memcpy(latest, crgb, sizeof(latest));
		sample_valid = true;

		if (streaming) {
			/*
			 * Teleplot format for the VS Code serial plotter: one
			 * ">name:value" per line. The LED level rides along as
			 * its own series so the stimulus and the response can
			 * be read off the same plot.
			 */
			LOG_PRINTK(">C:%d\n>R:%d\n>G:%d\n>B:%d\n>LED:%u\n", latest[0].val1,
				   latest[1].val1, latest[2].val1, latest[3].val1, led_pct);
		}

		if (ramping) {
			ramp_advance();
		}

		k_mutex_unlock(&colour_lock);
	}
}

/* Runs from boot: the sensor and LED are live as soon as the image starts. */
K_THREAD_DEFINE(colour_tid, 1024, colour_thread, NULL, NULL, NULL, 10, 0, 0);

static int cmd_start(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	k_mutex_lock(&colour_lock, K_FOREVER);
	streaming = true;
	k_mutex_unlock(&colour_lock);

	shell_print(sh, "colour streaming started");
	return 0;
}

static int cmd_stop(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	k_mutex_lock(&colour_lock, K_FOREVER);
	streaming = false;
	k_mutex_unlock(&colour_lock);

	shell_print(sh, "colour streaming stopped");
	return 0;
}

static int cmd_read(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct sensor_value crgb[4];
	uint8_t pct;
	bool valid;

	k_mutex_lock(&colour_lock, K_FOREVER);
	memcpy(crgb, latest, sizeof(crgb));
	pct = led_pct;
	valid = sample_valid;
	k_mutex_unlock(&colour_lock);

	if (!valid) {
		shell_error(sh, "no sample yet");
		return -EAGAIN;
	}

	shell_print(sh, "clear %d, red %d, green %d, blue %d (LED %u%%)", crgb[0].val1,
		    crgb[1].val1, crgb[2].val1, crgb[3].val1, pct);
	return 0;
}

static int cmd_ramp(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	k_mutex_lock(&colour_lock, K_FOREVER);
	ramping = true;
	k_mutex_unlock(&colour_lock);

	shell_print(sh, "LED ramp started");
	return 0;
}

static int cmd_led(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	char *end;
	long pct = strtol(argv[1], &end, 0);

	if (*end != '\0' || pct < 0 || pct > 100) {
		shell_error(sh, "brightness must be 0-100, got '%s'", argv[1]);
		return -EINVAL;
	}

	k_mutex_lock(&colour_lock, K_FOREVER);

	/* The ramp would move the level again on its next sample. */
	if (ramping) {
		ramping = false;
		shell_print(sh, "ramp stopped so the level stays put");
	}

	led_pct = (uint8_t)pct;

	int err = white_led_set_pct(led_pct);

	k_mutex_unlock(&colour_lock);

	if (err) {
		shell_error(sh, "failed to set brightness (%d)", err);
		return err;
	}

	shell_print(sh, "white LED at %ld%%", pct);
	return 0;
}

static int cmd_gain(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	char *end;
	long gain = strtol(argv[1], &end, 0);

	if (*end != '\0' || gain < 1 || gain > 64) {
		shell_error(sh, "gain must be 1, 4, 16 or 64, got '%s'", argv[1]);
		return -EINVAL;
	}

	struct sensor_value val = {.val1 = (int32_t)gain, .val2 = 0};
	int err = sensor_attr_set(sensor, SENSOR_CHAN_LIGHT, SENSOR_ATTR_GAIN, &val);

	if (err) {
		shell_error(sh, "failed to set gain (%d)", err);
		return err;
	}

	/* The driver rounds up to the nearest gain the hardware can do. */
	err = sensor_attr_get(sensor, SENSOR_CHAN_LIGHT, SENSOR_ATTR_GAIN, &val);
	if (err) {
		shell_error(sh, "failed to read back gain (%d)", err);
		return err;
	}

	shell_print(sh, "gain %dx", val.val1);
	return 0;
}

static int cmd_integration(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	char *end;
	long cycles = strtol(argv[1], &end, 0);

	if (*end != '\0' || cycles < 1 || cycles > 256) {
		shell_error(sh, "cycles must be 1-256, got '%s'", argv[1]);
		return -EINVAL;
	}

	struct sensor_value val = {.val1 = (int32_t)cycles, .val2 = 0};
	int err = sensor_attr_set(sensor, SENSOR_CHAN_LIGHT, SENSOR_ATTR_OVERSAMPLING, &val);

	if (err) {
		shell_error(sh, "failed to set integration cycles (%d)", err);
		return err;
	}

	shell_print(sh, "%ld integration cycles (%ld us)", cycles, cycles * 2780);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_colour, SHELL_CMD_ARG(start, NULL, "Start streaming colour samples", cmd_start, 1, 0),
	SHELL_CMD_ARG(stop, NULL, "Stop streaming colour samples", cmd_stop, 1, 0),
	SHELL_CMD_ARG(read, NULL, "Print the most recent sample", cmd_read, 1, 0),
	SHELL_CMD_ARG(ramp, NULL, "Resume the white LED ramp", cmd_ramp, 1, 0),
	SHELL_CMD_ARG(led, NULL,
		      "Stop the ramp and hold a brightness, 0 is off\n"
		      "Usage: colour led <0-100>",
		      cmd_led, 2, 0),
	SHELL_CMD_ARG(gain, NULL,
		      "Set the RGBC analog gain\n"
		      "Usage: colour gain <1|4|16|64>",
		      cmd_gain, 2, 0),
	SHELL_CMD_ARG(integration, NULL,
		      "Set the number of integration cycles\n"
		      "Usage: colour integration <1-256>",
		      cmd_integration, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(colour, &sub_colour, "TCS3400 colour sensing", NULL);
