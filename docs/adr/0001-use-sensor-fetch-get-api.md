# 1. Use the Fetch-and-Get sensor API for drivers

## Status

Accepted

## Context

Zephyr currently offers two ways for a driver to expose sensor data:

- **Fetch and Get** (`sensor_sample_fetch()` / `sensor_channel_get()`): the
  long-standing, stable API. A driver implements `sample_fetch` and
  `channel_get` in its `sensor_driver_api`; callers fetch a channel, then
  copy it out into a caller-owned `struct sensor_value` buffer.
- **Read and Decode** (`sensor_read()` / `sensor_decode()`): a newer,
  RTIO-based API where a driver implements `submit()` and exposes a
  `sensor_decoder_api`. Encoded samples are read into a buffer and decoded
  into `q31_t` values on demand, which also enables async/queued reads
  across multiple sensors.

Zephyr's own docs state that Fetch and Get is expected to be deprecated in
favor of Read and Decode, but as of this writing Fetch and Get is still the
stable, fully-documented, widely-used API, and Read and Decode is still
evolving (e.g. `struct sensor_chan_spec` work in progress).

Our drivers under `dev/lib/drivers/` (starting with mpu6050) are simple,
single-shot, non-FIFO I2C sensors with an optional GPIO data-ready
interrupt. They don't need RTIO's async/queued/streaming capabilities today.

## Decision

Implement `dev/lib/drivers/` sensor drivers against the Fetch-and-Get API
(`sensor_sample_fetch` / `sensor_channel_get`) for now.

## Consequences

- Drivers stay simple and match the API most Zephyr sample code and
  documentation still assumes.
- `sensor_channel_get()` takes a bare `struct sensor_value *`, so the
  compiler can't verify a caller's buffer is large enough for what a given
  channel writes (e.g. `SENSOR_CHAN_ALL` writes more values than
  `SENSOR_CHAN_ACCEL_XYZ`). Callers must size buffers correctly by hand;
  see `dev/samples/mpu6050/src/main.c` for the current mitigation
  (a per-channel wrapper with a sized array parameter).
- We are taking on migration work later: see
  [0002-migrate-to-read-decode-rtio-api.md](0002-migrate-to-read-decode-rtio-api.md).
