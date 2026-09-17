#!/usr/bin/env python3
"""Find which frequency bands separate an inhaler actuation from background.

    python3 imu_analyse.py actuations.npz

Takes a recording from imu_scope.py --record, locates the transients, and
compares their spectra against the quiet parts of the same recording. Output is
a per-band separation table plus a plot: pick bands from the top of the table.

Detection is deliberately crude -- a high-passed energy envelope over a median
threshold -- because its only job is to find windows to characterise. The point
of the exercise is what the table says about *which bands to build the real
detector on*, so use --show-events to check it framed the right moments before
trusting it.
"""

import argparse

import numpy as np


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("recording")
    p.add_argument("--axis", default="mag", choices=["x", "y", "z", "mag"])
    p.add_argument("--nfft", type=int, default=256)
    p.add_argument("--hop", type=int, default=64)
    p.add_argument("--hp", type=float, default=200.0,
                   help="high-pass corner (Hz) for the detection envelope only")
    p.add_argument("--k", type=float, default=8.0,
                   help="threshold = median + k * MAD of the envelope")
    p.add_argument("--pad", type=float, default=0.010,
                   help="seconds either side of a detection to call the event")
    p.add_argument("--bands", type=int, default=16, help="analysis bands")
    p.add_argument("--show-events", action="store_true",
                   help="plot the envelope and what was marked")
    p.add_argument("--vmin", type=float, default=-55.0,
                   help="dB re 1 m/s^2; fixed so runs are comparable "
                        "(noise floor ~-50dB)")
    p.add_argument("--vmax", type=float, default=10.0, help="top of the colour scale, dB")
    p.add_argument("--autoscale", action="store_true",
                   help="track the data instead of holding the scale fixed")
    p.add_argument("--save", metavar="FILE.png")
    p.add_argument("--no-show", action="store_true",
                   help="write --save and exit without opening a window")
    return p.parse_args()


def stft(sig, fs, nfft, hop):
    win = np.hanning(nfft)
    n = 1 + (len(sig) - nfft) // hop
    idx = np.arange(nfft)[None, :] + hop * np.arange(n)[:, None]
    seg = sig[idx]
    seg = seg - seg.mean(axis=1, keepdims=True)      # gravity out
    mag = np.abs(np.fft.rfft(seg * win, axis=1)) * (2.0 / win.sum())
    return np.fft.rfftfreq(nfft, 1 / fs), (np.arange(n) * hop + nfft / 2) / fs, mag


def main():
    args = parse_args()

    import matplotlib
    matplotlib.use("Agg" if args.no_show else "MacOSX")
    import matplotlib.pyplot as plt

    z = np.load(args.recording)
    a, fs = z["samples"], float(z["fs"])
    sig = np.linalg.norm(a, axis=1) if args.axis == "mag" else a[:, "xyz".index(args.axis)]
    sig = sig.astype(np.float64)
    dur = len(sig) / fs
    print(f"{args.recording}: {len(sig)} samples, {fs:.0f} Hz, {dur:.1f} s, axis={args.axis}")

    freqs, times, mag = stft(sig, fs, args.nfft, args.hop)

    # Envelope from the high-passed bins only: the actuation is an impact, and
    # anything below ~200Hz is hand motion and gravity wander.
    hp = freqs >= args.hp
    env = mag[:, hp].sum(axis=1)

    med = np.median(env)
    mad = np.median(np.abs(env - med)) or env.std() or 1e-12
    thresh = med + args.k * mad
    hot = env > thresh

    # Group contiguous hot columns into events, padded either side.
    pad_cols = max(1, int(args.pad * fs / args.hop))
    events, i = [], 0
    while i < len(hot):
        if hot[i]:
            j = i
            while j + 1 < len(hot) and hot[j + 1]:
                j += 1
            events.append((max(0, i - pad_cols), min(len(hot) - 1, j + pad_cols)))
            i = j + 1
        else:
            i += 1

    # Merge events that now overlap after padding.
    merged = []
    for s, e in events:
        if merged and s <= merged[-1][1]:
            merged[-1] = (merged[-1][0], e)
        else:
            merged.append((s, e))
    events = merged

    ev_mask = np.zeros(len(hot), dtype=bool)
    for s, e in events:
        ev_mask[s : e + 1] = True
    bg_mask = ~ev_mask

    print(f"threshold  : median {med:.4g} + {args.k}*MAD {mad:.4g} = {thresh:.4g}")
    print(f"events     : {len(events)}  covering {ev_mask.sum()/len(hot)*100:.1f}% of the record")
    if events:
        print("             at " + ", ".join(f"{times[s]:.2f}-{times[e]:.2f}s"
                                             for s, e in events[:12])
              + (" ..." if len(events) > 12 else ""))

    if not events or bg_mask.sum() < 8:
        print("\nNot enough separation to compare. Record a run containing several "
              "actuations and some quiet, or lower --k.")
        return

    # Per-band separation: event median vs background 95th percentile. Using the
    # background's upper tail rather than its mean is what makes the number a
    # usable detection margin instead of a flattering average.
    edges = np.linspace(0, fs / 2, args.bands + 1)
    print(f"\n{'band (Hz)':>18}  {'event':>10}  {'bg p95':>10}  {'margin dB':>9}")
    print("  " + "-" * 52)
    rows = []
    for b in range(args.bands):
        sel = (freqs >= edges[b]) & (freqs < edges[b + 1])
        if not sel.any():
            continue
        e_pow = mag[ev_mask][:, sel].sum(axis=1)
        b_pow = mag[bg_mask][:, sel].sum(axis=1)
        ev = np.median(e_pow)
        bg = np.percentile(b_pow, 95)
        margin = 20 * np.log10(max(ev, 1e-12) / max(bg, 1e-12))
        rows.append((margin, edges[b], edges[b + 1], ev, bg))
    rows.sort(reverse=True)
    for margin, lo, hi, ev, bg in rows:
        flag = "  <-- best" if margin == rows[0][0] else ""
        print(f"{lo:7.0f}-{hi:<7.0f}  {ev:10.4g}  {bg:10.4g}  {margin:+9.1f}{flag}")

    best = rows[0]
    print(f"\nStrongest separation: {best[1]:.0f}-{best[2]:.0f} Hz at {best[0]:+.1f} dB "
          f"above the background 95th percentile.")
    print("Bands within a few dB of it are worth combining -- a sum over two or "
          "three adjacent bands is usually steadier than the single best one.")

    fig, axes = plt.subplots(3 if args.show_events else 2, 1, figsize=(11, 8),
                             sharex=False)
    ax = iter(axes)

    a0 = next(ax)
    db = 20 * np.log10(np.maximum(mag.T, 1e-9))
    # Fixed scale by default: two recordings are only comparable by eye if the
    # same colour means the same level in both.
    if args.autoscale:
        lo, hi = np.percentile(db, 40), np.percentile(db, 99.95)
    else:
        lo, hi = args.vmin, args.vmax
    a0.imshow(db, origin="lower", aspect="auto", cmap="magma", vmin=lo, vmax=hi,
              extent=[times[0], times[-1], 0, fs / 2])
    for s, e in events:
        a0.axvspan(times[s], times[e], color="cyan", alpha=0.20)
    a0.set_xlabel("seconds")
    a0.set_ylabel("Hz")
    a0.set_title(f"{args.recording} -- spectrogram, {len(events)} events marked", fontsize=10)

    a1 = next(ax)
    band_mid = [(lo + hi) / 2 for _, lo, hi, _, _ in sorted(rows, key=lambda r: r[1])]
    band_m = [m for m, _, _, _, _ in sorted(rows, key=lambda r: r[1])]
    a1.bar(band_mid, band_m, width=(fs / 2) / args.bands * 0.9)
    a1.axhline(0, color="k", lw=0.8)
    a1.set_xlabel("Hz")
    a1.set_ylabel("margin dB")
    a1.set_title("event vs background separation per band", fontsize=10)
    a1.grid(alpha=0.3)

    if args.show_events:
        a2 = next(ax)
        a2.plot(times, env, lw=0.7, label=f"envelope >{args.hp:.0f}Hz")
        a2.axhline(thresh, color="r", lw=0.8, label="threshold")
        a2.set_xlabel("seconds")
        a2.legend(fontsize=8)
        a2.grid(alpha=0.3)

    fig.tight_layout()
    if args.save:
        fig.savefig(args.save, dpi=130)
        print(f"wrote {args.save}")
    if not args.no_show:
        plt.show()


if __name__ == "__main__":
    main()
