#!/usr/bin/env python3
"""Live scope for the lsm6dsv16x SPI stream, with pause-and-select FFT.

    python3 imu_scope.py                        # live
    python3 imu_scope.py --record run1.npz      # live + capture everything

Three panels: the waveform, a rolling spectrogram, and the spectrum of whatever
window you select.

    space   pause / resume
    drag    (while paused, on the waveform) select a window -> FFT of exactly
            that window, at full sample resolution
    c       clear the selection
    s       save the selected window to a .npz
    q       quit

The point of pausing is that an actuation is over in milliseconds. You watch
live, see one go past, freeze, then drag across just that transient and read its
spectrum without the surrounding background averaged into it.

Gravity is removed per window (the mean is subtracted before every FFT), so both
the colour scale and the spectrum belong to the vibration rather than to DC.
"""

import argparse
import os
import sys
import threading
import time
from typing import NamedTuple

import numpy as np

from imu_read import Stream

AXIS_IDX = {"x": 0, "y": 1, "z": 2}

SCALE_HELP = """bottom of the colour scale, dB re 1 m/s^2. Fixed by
                        default so a given colour means the same level in every
                        window and every run -- autoscaling makes identical
                        events look different depending on what else is on
                        screen. -55 puts the measured noise floor (~-50dB
                        median) at the dark end, and +10 leaves headroom above
                        a desk knock (~+6dB)."""


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default="/dev/cu.usbmodem0010518207903")
    p.add_argument("--baud", type=int, default=921600)
    p.add_argument("--axis", default="mag", choices=["x", "y", "z", "mag"],
                   help="mag = |a| - mean, which catches the transient whatever "
                        "its direction (default)")
    p.add_argument("--nfft", type=int, default=256,
                   help="spectrogram FFT length; 256 at 7200Hz is 36ms / 28Hz bins. "
                        "The selection FFT uses the whole selection instead.")
    p.add_argument("--hop", type=int, default=64, help="samples between columns")
    p.add_argument("--span", type=float, default=8.0,
                   help="seconds held on screen, and the most you can select")
    p.add_argument("--fmax", type=float, default=None, help="top of frequency axis (Hz)")
    p.add_argument("--vmin", type=float, default=-55.0, help=SCALE_HELP)
    p.add_argument("--vmax", type=float, default=10.0, help="top of the colour scale, dB")
    p.add_argument("--autoscale", action="store_true",
                   help="track the data instead of holding the scale fixed; for a "
                        "first look at an unknown signal, not for comparing runs")
    p.add_argument("--record", metavar="FILE.npz", help="also save every raw sample")
    p.add_argument("--no-plot", action="store_true",
                   help="record only, no window -- for headless or long captures")
    p.add_argument("--seconds", type=float,
                   help="stop after this long (default: until the window closes "
                        "or ctrl-C)")
    return p.parse_args()


class Reader(threading.Thread):
    """Serial stays on its own thread so rendering never stalls the link."""

    daemon = True

    def __init__(self, port, baud):
        super().__init__()
        self.stream = Stream(port, baud)
        self.q = []
        self.lock = threading.Lock()
        self.stop = False

    def run(self):
        while not self.stop:
            for samples, _ in self.stream.read():
                with self.lock:
                    self.q.append(samples)

    def drain(self):
        with self.lock:
            out, self.q = self.q, []
        return out


def derive(a, axis):
    """Per-axis signal, or |a| which catches the transient from any direction."""
    return (np.linalg.norm(a, axis=1) if axis == "mag"
            else a[:, AXIS_IDX[axis]]).astype(np.float32)


def envelope(sig, npix):
    """Min/max per pixel column.

    Plotting 8s * 7200Hz as a polyline is both slow and a lie -- matplotlib
    would decimate by dropping samples, so a 1ms transient could vanish between
    pixels. Min/max per column keeps every peak visible at any zoom.
    """
    n = len(sig)
    if n < npix * 2:
        return np.arange(n), sig
    per = n // npix
    trimmed = sig[: per * npix].reshape(npix, per)
    lo, hi = trimmed.min(axis=1), trimmed.max(axis=1)
    x = np.repeat(np.arange(npix) * per + per / 2.0, 2)
    y = np.empty(npix * 2, dtype=sig.dtype)
    y[0::2], y[1::2] = lo, hi
    return x, y


def zoom_axis(lo, hi, centre, factor, limit=None):
    """Scale a range about `centre`, clamped to `limit` if given."""
    lo = centre - (centre - lo) * factor
    hi = centre + (hi - centre) * factor
    if limit is not None:
        lo, hi = max(lo, limit[0]), min(hi, limit[1])
        if hi - lo < (limit[1] - limit[0]) * 1e-4:      # do not collapse to nothing
            return None
    return lo, hi


class Spectrum(NamedTuple):
    """One selection, in the three forms the panels and the comparison need."""

    f: np.ndarray           # Hz
    db: np.ndarray          # amplitude dB re 1 m/s^2 -- the spectrogram's scale
    psd: np.ndarray         # (m/s^2)^2 / Hz
    power: np.ndarray       # (m/s^2)^2 per bin
    peak: int
    rms: float
    centroid: float         # where the energy sits in the window, 0..1
    meta: dict = None       # duration, sample count, selected span


def selection_spectrum(seg, fs):
    """One FFT over the whole selection -- no averaging.

    Averaging sub-windows would smear a millisecond transient against the
    background either side of it, which is exactly what selecting it was for.

    Three normalisations come back, because they answer different questions and
    cannot be had from one scaling:

      db     amplitude, coherent gain (2/sum w), then 20*log10. A tone of
             amplitude A peaks at 20*log10(A). Deliberately identical to the
             spectrogram's colour scale, so a level read off one matches the
             other.
      power  mean-square per bin, noise gain (n * sum w^2). The bins sum to the
             signal's total mean-square, so a band total is meaningful.
      psd    power per unit bandwidth, psd = power/df. Unlike power per bin it
             does not change with the window length, so it is what compares
             selections of different durations.

    The db and power conventions differ by a fixed 10*log10(3) = 4.77dB for a
    tone; that is the cost of having each be correct for its own purpose.
    """
    n = len(seg)
    w = np.hanning(n)
    x = seg.astype(np.float64) - np.mean(seg)
    spec = np.fft.rfft(x * w)
    mag2 = np.abs(spec) ** 2

    # One-sided: fold the negative frequencies in by doubling, except DC and
    # (for even n) Nyquist, which have no mirror image.
    fold = np.full(mag2.shape, 2.0)
    fold[0] = 1.0
    if n % 2 == 0:
        fold[-1] = 1.0

    amp = np.abs(spec) * (2.0 / w.sum())
    db = 20 * np.log10(np.maximum(amp, 1e-12))
    power = fold * mag2 / (n * np.sum(w ** 2))
    psd = power * n / fs

    # Where the energy sits inside the window, 0..1. The Hann taper is ~0 at
    # the edges, so a transient near one is largely deleted before the
    # transform ever sees it -- measured at 0.00x its true power for a burst at
    # 6% in, against 2.67x for the same burst centred.
    e = x ** 2
    total = e.sum()
    centroid = float((e * np.arange(n)).sum() / total / n) if total > 0 else 0.5

    return Spectrum(f=np.fft.rfftfreq(n, 1.0 / fs), db=db, psd=psd, power=power,
                    peak=int(np.argmax(power[1:]) + 1), rms=float(np.sqrt(np.mean(e))),
                    centroid=centroid)


def record_only(args, reader):
    """No window: drain to disk until --seconds elapses or ctrl-C."""
    blocks, n, t0 = [], 0, None
    last_print = 0.0
    try:
        while True:
            got = reader.drain()
            if got:
                if t0 is None:
                    t0 = time.time()
                    got = []          # start timing at the first frame
                blocks += got
                n += sum(len(b) for b in got)
            if t0 and args.seconds and time.time() - t0 >= args.seconds:
                break
            if t0 is None and time.time() > args.deadline:
                sys.exit(f"no frames on {args.port}")
            time.sleep(0.02)
            if time.time() - last_print > 1.0:
                last_print = time.time()
                print(f"\r{n} samples  frames {reader.stream.frames}  "
                      f"dropped {reader.stream.dropped}  bad {reader.stream.bad}",
                      end="" if sys.stdout.isatty() else "\n", flush=True)
    except KeyboardInterrupt:
        pass

    reader.stop = True
    if not blocks:
        sys.exit("\nnothing captured")
    a = np.concatenate(blocks)
    fs = n / (time.time() - t0)
    np.savez_compressed(args.record, samples=a, fs=fs, axes=np.array(["x", "y", "z"]))
    print(f"\nwrote {args.record}: {len(a)} samples, {fs:.0f} Hz measured")


def measure_rate(args, reader):
    """The part clocks ~6% under its nominal ODR, and the whole frequency axis
    hangs off this, so measure it rather than trusting the devicetree.

    The port is flushed on open and the first half second is discarded: a
    freshly opened port can spend its first moment re-syncing, which reads ~10%
    low, and a wrong sample rate mislabels every frequency downstream without
    ever looking wrong. With those two, repeat measurements agree to ~0.3%.
    """
    SETTLE, WINDOW = 0.5, 2.0
    deadline = time.time() + 8 + SETTLE + WINDOW
    t_first, t0, t1, n = None, None, None, 0
    bad0 = dropped0 = 0

    while time.time() < deadline:
        blocks = reader.drain()
        got = sum(len(b) for b in blocks)

        if t_first is None:
            if got:
                t_first = time.time()      # discard this drain entirely
            time.sleep(0.02)
            continue

        now = time.time()
        if now - t_first < SETTLE:         # let it settle, count nothing
            time.sleep(0.02)
            continue

        if t0 is None:
            t0, n = now, 0
            bad0, dropped0 = reader.stream.bad, reader.stream.dropped
        n += got
        t1 = now
        if t1 - t0 >= WINDOW and n:
            break
        time.sleep(0.02)

    if not n:
        sys.exit(f"no frames on {args.port} -- is the board running and the port free?")

    # Frame loss during warmup is a real signal that the link is degraded;
    # comparing sub-windows is not -- USB delivers in bursts, so healthy
    # half-second rates scatter by several percent on their own.
    lost = (reader.stream.bad - bad0, reader.stream.dropped - dropped0)
    if any(lost):
        print(f"warning: {lost[0]} corrupt and {lost[1]} dropped frames while "
              "measuring -- the rate and everything derived from it may be off.")

    return n / (t1 - t0)


def main():
    args = parse_args()

    if args.no_plot and not args.record:
        sys.exit("--no-plot needs --record, otherwise it does nothing")

    reader = Reader(args.port, args.baud)
    reader.start()
    args.deadline = time.time() + 5

    if args.no_plot:
        record_only(args, reader)
        return

    import matplotlib
    # MPLBACKEND wins if set, so this runs headless (Agg) for a smoke test or
    # over a connection without a window server.
    if not os.environ.get("MPLBACKEND"):
        matplotlib.use("MacOSX")
    import matplotlib.pyplot as plt
    from matplotlib.widgets import Button, SpanSelector

    fs = measure_rate(args, reader)
    print(f"measured sample rate: {fs:.0f} Hz  (Nyquist {fs/2:.0f} Hz)")
    print("space pause | drag to select (paused) -> power + PSD of that window | "
          "scroll zoom | r reset | c clear | s save | q quit")

    nfft, hop = args.nfft, args.hop
    nbins = nfft // 2 + 1
    cols = max(16, int(args.span * fs / hop))
    win = np.hanning(nfft).astype(np.float32)
    winsum = win.sum()
    fmax = args.fmax or fs / 2

    # Full-rate ring of the last --span seconds. The waveform is drawn
    # decimated, but selections index this, so an FFT always sees every sample.
    nring = int(args.span * fs)
    ring = np.zeros(nring, dtype=np.float32)
    ring_raw = np.zeros((nring, 3), dtype=np.float32)
    filled = 0

    spec = np.full((nbins, cols), args.vmin, dtype=np.float32)
    pending = np.zeros(0, dtype=np.float32)
    recording = [] if args.record else None

    state = {"paused": False, "sel": None, "frozen": None, "wave_zoom": False,
             "cur": None, "prev": None, "window": None, "syncing": False}

    fig = plt.figure(figsize=(12, 9))
    # The colourbar gets its own column rather than being attached to the
    # spectrogram. Attached, it is taken out of that axes' width alone, which
    # leaves the waveform and spectrogram plot boxes different widths and the
    # time axes visibly out of step down the page.
    # Nested, so the two time plots sit tight against one another for reading
    # vertically while the spectra below still clear the time axis label.
    outer = fig.add_gridspec(2, 2, height_ratios=[3.0, 1.35],
                             width_ratios=[1, 0.022], hspace=0.30, wspace=0.025)
    top = outer[0, 0].subgridspec(2, 1, height_ratios=[1, 2], hspace=0.17)
    ax_w = fig.add_subplot(top[0])
    ax_s = fig.add_subplot(top[1])
    # Colourbar spans only the spectrogram's rows, so it lines up with it.
    cax = fig.add_subplot(outer[0, 1].subgridspec(2, 1, height_ratios=[1, 2],
                                                  hspace=0.17)[1])
    bot = outer[1, 0].subgridspec(1, 2, wspace=0.16)
    ax_fa = fig.add_subplot(bot[0])
    ax_fb = fig.add_subplot(bot[1])
    ax_f = ax_fa                      # the pair share limits and handling
    fig.canvas.manager.set_window_title("lsm6dsv16x SPI -- inhaler actuation")

    # Deliberately NOT sharex. The waveform is the navigator: it always shows
    # the whole ring and highlights the window the spectrogram is zoomed into,
    # so the selection can be dragged around on it while the detail view
    # follows. Both still measure x in seconds before now, so a coordinate
    # means the same thing on either.

    (wave,) = ax_w.plot([], [], lw=0.6, color="tab:blue")
    ax_w.set_xlim(-args.span, 0)
    ax_w.set_ylabel(f"{args.axis}  m/s$^2$")
    ax_w.set_title("", fontsize=9, loc="left")
    ax_w.tick_params(labelbottom=True, labelsize=8)
    ax_w.grid(alpha=0.3)

    img = ax_s.imshow(spec, origin="lower", aspect="auto", cmap="magma",
                      extent=[-args.span, 0, 0, fs / 2],
                      vmin=args.vmin, vmax=args.vmax)
    ax_s.set_ylim(0, fmax)
    ax_s.set_xlabel("seconds (now at right)")
    ax_s.set_ylabel("Hz")
    cb = fig.colorbar(img, cax=cax)
    cb.set_label("dB re 1 m/s$^2$", fontsize=8)
    cax.tick_params(labelsize=8)

    # Left: dB, on the same fixed scale as the spectrogram's colour axis, so a
    # level read off one matches the other. Right: power spectral density,
    # linear. Power per bin is not plotted -- it is the same curve as the
    # density, differing only by the constant df, so it told us nothing new.
    # The previous selection stays on both as a grey ghost.
    FFT_COLOURS = ("tab:purple", "tab:orange")
    fft_axes, flines, ghosts = [ax_fa, ax_fb], [], []
    for i, ax in enumerate(fft_axes):
        (ghost,) = ax.plot([], [], lw=0.8, color="0.6", alpha=0.7)
        (line,) = ax.plot([], [], lw=0.9, color=FFT_COLOURS[i])
        ghosts.append(ghost)
        flines.append(line)
        ax.set_xlim(0, fmax)
        ax.set_xlabel("Hz")
        ax.grid(alpha=0.3)
    ax_fa.set_ylabel("dB re 1 m/s$^2$")
    ax_fa.set_ylim(args.vmin, args.vmax)
    ax_fb.set_ylabel("PSD  (m/s$^2$)$^2$/Hz")
    fig.subplots_adjust(left=0.075, right=0.915, top=0.955, bottom=0.095)

    def current():
        """The samples on screen: frozen at pause, else the live ring."""
        if state["frozen"] is not None:
            return state["frozen"]
        n = min(filled, nring)
        return ring[nring - n:], ring_raw[nring - n:]

    def refresh_ffts():
        """Left = dB (fixed scale, matches the spectrogram), right = PSD
        (linear, autoscaled); the previous selection stays as a grey ghost."""
        cur, prev = state["cur"], state["prev"]

        def label(sp, tag):
            """Name a curve by the window it came from -- which is the only
            thing that tells the two apart once they overlap."""
            if sp is None or sp.meta is None:
                return "_nolegend_"
            t0, t1 = sp.meta["t"]
            return f"{tag}  {t0:+.2f}..{t1:+.2f}s  ({sp.meta['dur']*1000:.0f} ms)"

        for ax, line, ghost, field in zip(fft_axes, flines, ghosts, ("db", "psd")):
            line.set_data(*((cur.f, getattr(cur, field)) if cur else ([], [])))
            ghost.set_data(*((prev.f, getattr(prev, field)) if prev else ([], [])))
            line.set_label(label(cur, "this"))
            ghost.set_label(label(prev, "prev"))

            old_leg = ax.get_legend()
            # Explicit handle order: the ghost is drawn first so it sits under
            # the current curve, but it should be listed second.
            handles = ([line] if cur else []) + ([ghost] if prev else [])
            if handles:
                ax.legend(handles=handles, loc="upper right", fontsize=7,
                          framealpha=0.85, handlelength=1.4, borderpad=0.4,
                          labelspacing=0.3)
            elif old_leg:
                old_leg.remove()

        # dB keeps the fixed scale so panels and colourbar mean the same thing.
        ax_fa.set_ylim(args.vmin, args.vmax)
        if cur:
            # Density has no natural fixed range, so it tracks the data -- but
            # from zero, since it cannot be negative.
            ax_fb.set_autoscaley_on(True)
            ax_fb.relim()
            ax_fb.autoscale_view(scalex=False)
            ax_fb.set_ylim(bottom=0)
        else:
            ax_fb.set_ylim(0, 1)

        if cur:
            m = cur.meta
            # Total power as well as the peak: quoting only a peak bin
            # understates a transient spread across many bins.
            ax_fa.set_title(f"spectrum -- {m['dur']*1000:.1f} ms, peak "
                            f"{cur.f[cur.peak]:.0f} Hz at {cur.db[cur.peak]:.1f} dB",
                            fontsize=8, loc="left")
            ax_fb.set_title(f"power spectral density -- bin {cur.f[1]:.1f} Hz, total "
                            f"power {cur.power.sum():.4g} (m/s$^2$)$^2$",
                            fontsize=8, loc="left")
        else:
            ax_fa.set_title("spectrum -- pause and drag on a time plot",
                            fontsize=8, loc="left", color="0.4")
            ax_fb.set_title("power spectral density", fontsize=8, loc="left", color="0.4")

    def compare(nbands=12):
        """Current vs previous selection, band by band.

        Compared as PSD, not power: the two selections are rarely the same
        length, and power per bin scales with that length while density does
        not. Using power here would report a difference that is only the drag.
        """
        cur, prev = state["cur"], state["prev"]
        if cur is None or prev is None:
            return
        edges = np.linspace(0, fs / 2, nbands + 1)
        rows = []
        for k in range(nbands):
            sa = (cur.f >= edges[k]) & (cur.f < edges[k + 1])
            sb = (prev.f >= edges[k]) & (prev.f < edges[k + 1])
            if not sa.any() or not sb.any():
                continue
            pa, pb = cur.psd[sa].mean(), prev.psd[sb].mean()
            if pb <= 0:
                continue
            rows.append((10 * np.log10(pa / pb), edges[k], edges[k + 1]))
        rows.sort(key=lambda r: -abs(r[0]))
        print("   this selection vs the previous one (positive = stronger now):")
        for d, lo, hi in rows[:5]:
            print(f"     {lo:7.0f}-{hi:<7.0f} Hz  {d:+6.1f} dB")

    def on_select(x0, x1):
        """A time window was chosen -- from the navigator or the detail view.
        Either way: zoom the spectrogram to it, mark it on the navigator, and
        transform exactly that span."""
        if not state["paused"] or state["syncing"]:
            return
        sig, raw = current()
        # x is seconds before now; the last sample sits at t=0.
        i0, i1 = sorted((int(round(x0 * fs)) + len(sig) - 1,
                         int(round(x1 * fs)) + len(sig) - 1))
        want = i1 - i0
        i0, i1 = max(0, i0), min(i1, len(sig))

        # Until the ring fills, the axis is wider than the history behind it, so
        # a drag can land partly or wholly before the data starts. Clamping
        # silently would hand back a few samples and an FFT that means nothing.
        MIN = 32
        if i1 - i0 < MIN:
            print(f"selection is outside the {len(sig)/fs:.1f}s of data held so far "
                  f"-- drag within the filled part of the trace")
            return
        if i1 - i0 < want * 0.9:
            print(f"note: selection clipped to the {(i1-i0)/fs*1000:.1f} ms actually "
                  f"held (asked for {want/fs*1000:.1f} ms)")

        seg = sig[i0:i1]
        state["sel"] = (i0, i1)

        sp = selection_spectrum(seg, fs)
        dur = len(seg) / fs
        if not 0.3 < sp.centroid < 0.7:
            where = "start" if sp.centroid <= 0.3 else "end"
            print(f"   warning: the energy sits near the {where} of this selection "
                  f"({sp.centroid*100:.0f}% in). The window tapers to zero at the "
                  "edges, so its power is being cut -- centre it and re-select.")
        state["prev"] = state["cur"]
        state["cur"] = sp._replace(meta=dict(dur=dur, n=len(seg), t=(x0, x1)))
        refresh_ffts()

        # Zoom the detail view to the window; leave the navigator showing the
        # whole ring with the window marked on it.
        state["window"] = (x0, x1)
        ax_s.set_xlim(x0, x1)
        ax_w.set_xlim(*full["t"])

        # The navigator's own selector is the highlight, and stays live so its
        # edges can be dragged to move or resize the window.
        state["syncing"] = True
        try:
            selector.extents = (x0, x1)
            selector.set_visible(True)
        finally:
            state["syncing"] = False

        # Clear the overlay off the spectrogram so the zoomed image is clean.
        rect.set_visible(False)
        print(f"{dur*1000:7.1f} ms  {len(seg):6d} samples  peak {sp.f[sp.peak]:7.0f} Hz "
              f"at {sp.db[sp.peak]:+.1f} dB  total power {sp.power.sum():.4g} "
              f"(m/s^2)^2  RMS {sp.rms:.4f} m/s^2")
        compare()
        fig.canvas.draw_idle()

    selector = SpanSelector(ax_w, on_select, "horizontal", useblit=True,
                            props=dict(alpha=0.25, facecolor="tab:cyan"),
                            interactive=True, drag_from_anywhere=True)
    selector.set_active(False)

    # Time only, exactly like the waveform: dragging on the spectrogram picks a
    # span of time, not a box. The frequency axis is for looking at, and is
    # changed by zooming rather than by selecting.
    rect = SpanSelector(ax_s, on_select, "horizontal", useblit=True,
                        props=dict(alpha=0.25, facecolor="tab:cyan"),
                        interactive=True, drag_from_anywhere=True)
    rect.set_active(False)

    def on_key(ev):
        if ev.key == " ":
            state["paused"] = not state["paused"]
            selector.set_active(state["paused"])
            rect.set_active(state["paused"])
            if state["paused"]:
                sig, raw = current()
                state["frozen"] = (sig.copy(), raw.copy())
            else:
                state["frozen"] = None
                state["sel"] = None
                # A window is "seconds before now", so once data moves again it
                # no longer points at what was selected. Back to the full view.
                reset_view()
            fig.canvas.draw_idle()
        elif ev.key == "c":
            state["sel"] = None
            state["cur"] = state["prev"] = None
            selector.set_visible(False)
            rect.set_visible(False)
            refresh_ffts()
            fig.canvas.draw_idle()
        elif ev.key == "s" and state["sel"]:
            i0, i1 = state["sel"]
            _, raw = current()
            name = f"selection_{time.strftime('%H%M%S')}.npz"
            np.savez_compressed(name, samples=raw[i0:i1], fs=fs,
                                axes=np.array(["x", "y", "z"]))
            print(f"wrote {name}: {i1-i0} samples ({(i1-i0)/fs*1000:.1f} ms)")
        elif ev.key == "r":
            reset_view()
        elif ev.key == "q":
            plt.close(fig)

    full = {"t": (-args.span, 0), "s_y": (0, fmax), "f_x": (0, fmax)}

    def reset_view():
        state["window"] = None
        ax_w.set_xlim(*full["t"])
        ax_s.set_xlim(*full["t"])
        selector.set_visible(False)
        rect.set_visible(False)
        ax_w.set_autoscaley_on(True)
        ax_s.set_ylim(*full["s_y"])
        for ax in fft_axes:
            ax.set_xlim(*full["f_x"])
            ax.autoscale(axis="y")
        state["wave_zoom"] = False
        fig.canvas.draw_idle()

    def on_scroll(ev):
        """Scroll zooms about the cursor. On the spectrogram, plain scroll
        works the frequency axis (the usual need) and shift works time."""
        if ev.inaxes is None:
            return
        factor = 1 / 1.25 if ev.button == "up" else 1.25
        shift = ev.key is not None and "shift" in ev.key
        ax = ev.inaxes

        if ax is ax_s:
            if shift:
                r = zoom_axis(*ax.get_xlim(), ev.xdata, factor, full["t"])
                if r:
                    ax.set_xlim(*r)
            else:
                r = zoom_axis(*ax.get_ylim(), ev.ydata, factor, full["s_y"])
                if r:
                    ax.set_ylim(*r)
        elif ax in fft_axes:
            r = zoom_axis(*ax.get_xlim(), ev.xdata, factor, full["f_x"])
            if r:
                for a in fft_axes:     # keep the pair comparable
                    a.set_xlim(*r)
        elif ax is ax_w:
            # x is fixed here -- the navigator always shows the whole ring, so
            # there is always somewhere to drag the window to.
            r = zoom_axis(*ax.get_ylim(), ev.ydata, factor)
            if r:
                ax.set_ylim(*r)
                # Stop fighting the user: once the amplitude axis is set by
                # hand, live updates must leave it alone.
                state["wave_zoom"] = True
        fig.canvas.draw_idle()

    # On-screen controls: the keyboard shortcuts stay, but reset in particular
    # is no use to anyone who has to already know it exists.
    b_reset = Button(fig.add_axes([0.86, 0.008, 0.12, 0.03]), "Reset view (r)")
    b_reset.on_clicked(lambda _ev: reset_view())
    b_pause = Button(fig.add_axes([0.72, 0.008, 0.12, 0.03]), "Pause (space)")

    def toggle_pause(_ev=None):
        on_key(type("E", (), {"key": " "})())
        b_pause.label.set_text("Resume (space)" if state["paused"] else "Pause (space)")
        fig.canvas.draw_idle()

    b_pause.on_clicked(toggle_pause)

    fig.canvas.mpl_connect("scroll_event", on_scroll)
    fig.canvas.mpl_connect("key_press_event", on_key)

    # Test seam: the pause/select handlers are closures over the plotting
    # state, so expose them for the self-test rather than leave the interactive
    # path unexercised. Nothing in normal operation reads this.
    fig._imu_test = {"key": on_key, "select": on_select, "state": state,
                     "selector": selector, "nring": nring, "fs": fs,
                     "scroll": on_scroll, "axes": (ax_w, ax_s, ax_f),
                     "rect": rect, "reset": reset_view,
                     "fft_axes": fft_axes, "refresh": refresh_ffts,
                     "spectrum": selection_spectrum}

    refresh_ffts()          # label the panels before anything is selected
    plt.show(block=False)

    nsamp, start, last_title = 0, time.time(), 0.0
    stop_at = start + args.seconds if args.seconds else None

    try:
        while plt.fignum_exists(fig.number):
            if stop_at and time.time() >= stop_at:
                break

            blocks = reader.drain()
            if blocks:
                a = np.concatenate(blocks)
                if recording is not None:
                    recording.append(a)
                nsamp += len(a)
                sig = derive(a, args.axis)

                # Keep filling the ring while paused so the link never stalls;
                # only the *display* is frozen.
                k = min(len(sig), nring)
                ring[:-k] = ring[k:]
                ring[-k:] = sig[-k:]
                ring_raw[:-k] = ring_raw[k:]
                ring_raw[-k:] = a[-k:]
                filled += len(sig)

                pending = np.concatenate([pending, sig])
                ncol = 0
                while len(pending) >= nfft:
                    seg = pending[:nfft]
                    seg = seg - seg.mean()          # gravity / DC out
                    mag = np.abs(np.fft.rfft(seg * win)) * (2.0 / winsum)
                    spec[:, :-1] = spec[:, 1:]
                    spec[:, -1] = 20.0 * np.log10(np.maximum(mag, 1e-9))
                    pending = pending[hop:]
                    ncol += 1

                if ncol and not state["paused"]:
                    img.set_data(spec)
                    if args.autoscale:
                        lo = np.percentile(spec, 40)
                        img.set_clim(lo, max(np.percentile(spec, 99.9), lo + 12))
                    shown, _ = current()
                    x, y = envelope(shown, 2000)
                    wave.set_data((x - (len(shown) - 1)) / fs, y)
                    if not state["wave_zoom"]:
                        ax_w.relim()
                        ax_w.autoscale_view(scalex=False)

            now = time.time()
            if now - last_title > 1.0:
                rate = nsamp / (now - start)
                ax_w.set_title(
                    ("PAUSED -- drag to select, space to resume   "
                     if state["paused"] else "")
                    + f"{rate:.0f} Hz   frames {reader.stream.frames}   "
                    f"dropped {reader.stream.dropped}   bad {reader.stream.bad}"
                    + (f"   recorded {nsamp}" if recording is not None else ""),
                    fontsize=9, loc="left")
                last_title = now

            plt.pause(0.03)
    except KeyboardInterrupt:
        pass
    finally:
        reader.stop = True
        if recording:
            a = np.concatenate(recording)
            np.savez_compressed(args.record, samples=a, fs=fs,
                                axes=np.array(["x", "y", "z"]))
            print(f"\nwrote {args.record}: {len(a)} samples, {fs:.0f} Hz")


if __name__ == "__main__":
    main()
