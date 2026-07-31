# 2. Migrate drivers to the Read-and-Decode (RTIO) sensor API

## Status

Proposed

## Context

See [0001-use-sensor-fetch-get-api.md](0001-use-sensor-fetch-get-api.md) for
why our drivers currently use Fetch and Get.

Zephyr's documentation states that Fetch and Get is expected to be
deprecated in favor of Read and Decode (`sensor_read()` / `sensor_decode()`,
built on RTIO). The new API fixes a real problem we hit in the mpu6050
driver: `sensor_channel_get()`'s bare `struct sensor_value *` parameter
gives the compiler no way to check that a caller's buffer is large enough
for what a channel writes, so a mismatch (e.g. requesting
`SENSOR_CHAN_ALL` into a buffer sized for `SENSOR_CHAN_ACCEL_XYZ`) silently
overflows the buffer instead of failing loudly. Read and Decode buffers
carry their own size/type info and the decoder is expected to validate
against the caller's buffer before writing.

This is a real migration, not a drop-in API swap: it requires the driver to
implement `submit()` and a `sensor_decoder_api` (decode, get_frame_count,
get_size_info, has_trigger, ...), define an encoded-data buffer layout, and
reconcile our existing GPIO data-ready trigger handling with RTIO's
request/completion model.

## Decision

Not yet made. This ADR exists to record the intent: once the Read and
Decode API and its `sensor_chan_spec` work stabilize in an upstream Zephyr
release we track, migrate `dev/lib/drivers/` sensor drivers (starting with
mpu6050) from Fetch and Get to Read and Decode.

## Consequences

- Until this migration happens, buffer-size mismatches with
  `sensor_channel_get()` remain a manual/reviewer concern rather than a
  compiler- or driver-enforced one.
- When we do migrate, sample application code (e.g.
  `dev/samples/mpu6050/src/main.c`) will also need to move from
  `sensor_sample_fetch`/`sensor_channel_get` to `sensor_read`/
  `sensor_decode`.
- Revisit this ADR before starting the migration to confirm the upstream
  API surface we plan to depend on hasn't changed further.
