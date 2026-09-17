"""Frame reader for the lsm6dsv16x_spi binary stream.

Wire format is documented in ../src/main.c. Shared by imu_scope.py (live) and
imu_analyse.py (offline), so the parser exists in exactly one place.
"""

import struct

import numpy as np
import serial

MAGIC = b"\xa5\x5a\xa5\x5a"  # 0x5AA55AA5 little-endian
HDR = struct.Struct("<IIHbB")  # magic, seq, n, shift, axes
AXES = 3


class Stream:
    """Resynchronising reader. Yields (samples_m_s2, seq, dropped) per frame."""

    def __init__(self, port, baud=921600, timeout=0.2):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        # Drop whatever the OS buffered before we opened: it is a backlog of
        # unknown age, and timing against it skews any rate measured at startup.
        self.ser.reset_input_buffer()
        self.buf = bytearray()
        self.prev_seq = None
        self.frames = 0
        self.bad = 0
        self.dropped = 0

    def close(self):
        self.ser.close()

    def _frames_in_buf(self):
        while True:
            i = self.buf.find(MAGIC)
            if i < 0:
                # Keep a magic-length tail; the rest is text or garbage.
                del self.buf[: max(0, len(self.buf) - 3)]
                return
            if i:
                del self.buf[:i]
            if len(self.buf) < HDR.size:
                return

            _, seq, n, shift, axes = HDR.unpack_from(self.buf)
            if axes != AXES or not 0 < n <= 256:
                # Magic appeared inside payload; step over it and re-sync.
                del self.buf[:4]
                self.bad += 1
                continue

            body = n * axes * 2
            total = HDR.size + body + 2
            if len(self.buf) < total:
                return

            data = bytes(self.buf[HDR.size : HDR.size + body])
            want = struct.unpack_from("<H", self.buf, HDR.size + body)[0]
            if (sum(data) & 0xFFFF) != want:
                del self.buf[:4]
                self.bad += 1
                continue

            del self.buf[:total]
            self.frames += 1

            if self.prev_seq is not None:
                gap = (seq - self.prev_seq - 1) & 0xFFFFFFFF
                if gap:
                    self.dropped += gap
            self.prev_seq = seq

            raw = np.frombuffer(data, dtype="<i2").reshape(n, axes)
            # m/s^2 = sample * 2^shift / 32768
            yield raw.astype(np.float32) * (2.0**shift / 32768.0), seq

    def read(self):
        chunk = self.ser.read(65536)
        if chunk:
            self.buf.extend(chunk)
        yield from self._frames_in_buf()
