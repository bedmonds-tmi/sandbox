# Actuation analysis tools

Host-side tooling for the `lsm6dsv16x_spi` sample, for working out which
frequency bands an inhaler cartridge actuation lands in.

The board streams **raw** accelerometer samples as framed binary over the
console UART -- 7680Hz nominal (~7200Hz measured), 3 axes, Nyquist ~3600Hz. No
filtering happens on-device: the actuation is a broadband mechanical impulse and
shaping it in firmware would be shaping the evidence. The frame format lives in
`../src/main.c` and is parsed in exactly one place, `imu_read.py`.

Because the UART now carries binary, these tools *are* the console. The I2C
sample (`../../lsm6dsv16x`) still prints human-readable lines if you want them.

## Live view

```
python3 imu_scope.py                          # waveform + spectrogram + selection FFT
python3 imu_scope.py --axis z --fmax 2000     # one axis, zoomed
```

Four panels: the waveform, a rolling time-vs-frequency plot, and two spectrum
panels for the current selection -- **dB** on the left, **power spectral
density** on the right.

| | |
|---|---|
| `space` / **Pause** button | pause / resume |
| drag on **either** time plot | while paused: select a span of time -> spectrogram zooms to it, FFT of exactly that window. Time only on both plots, never a box |
| drag the **highlight** | on the waveform: move or resize the window; the spectrogram follows |
| `r` / **Reset view** button | back to the full 8s and full band |
| scroll | zoom about the cursor. On the spectrogram: **frequency**; hold shift for **time** |
| toolbar | the magnifier gives a precise rectangle zoom; the house button also resets |
| `c` | clear both panels |
| `s` | save the selected window to `selection_HHMMSS.npz` |
| `q` | quit |

### dB and power spectral density

Both panels describe the same selection:

| | units | scale | what it is for |
|---|---|---|---|
| left, **dB** | dB re 1 m/s² | fixed, -55..+10 | wide dynamic range -- the noise floor and the peaks at once |
| right, **PSD** | (m/s²)²/Hz | linear, from 0 | peaks in proportion; comparable between selections |

The left panel uses **exactly the spectrogram's colour scale**, same convention
and same fixed limits, so a level read off the image matches the level read off
the curve. Verified equal to 2e-6 dB (float32 vs float64 rounding, not a scale
difference). A tone of amplitude A peaks at 20·log10(A).

The right panel is a **density**: power per unit bandwidth, `psd = power/df`.
That normalisation is what makes two selections of different durations
comparable -- power per bin scales with how long you dragged, density does not.
Its total power is quoted in the panel title.

Power per bin is computed but not plotted: it is the same curve as the density
differing only by the constant `df`, so a third panel of it showed nothing new.
It is what the title's "total power" and the band comparison are built from.

The two conventions differ by a fixed 10·log10(3) = 4.77 dB for a tone, because
each is correct for a different thing: dB uses the window's coherent gain
(`(Σw)²`, so a tone's peak bin reads its true amplitude), power uses its noise
gain (`n·Σw²`, so the bins sum to the true total). With noise gain a Hann window
spreads a tone over about three bins and it is their **sum** that comes to A²/2
-- so read a band, not a bin, for a tone's power. Checked: a tone's 5-bin sum
lands on A²/2 exactly, `∫PSD·df` equals the variance regardless of window
length, and the bins sum to the mean-square in expectation (mean ratio 1.0000
over 60 runs; a single selection scatters a few percent, shrinking as 1/√n).

#### Centre the transient in the selection

The Hann window tapers to zero at both edges, so a click near the edge of your
selection is attenuated before the transform sees it. Measured, for the same
burst in the same length of window:

| position in window | measured power vs true |
|---|---|
| centred | 2.67× |
| a quarter in | 0.67× |
| 6% in (near edge) | 0.00× |

The tool warns when the selection's energy centroid falls outside the middle
40%. Take it seriously — an off-centre transient produces a confident-looking
and completely wrong number.

### Comparing with the previous selection

The previous selection stays on both panels as a grey ghost. Each panel is
labelled with the window each curve came from:

```
this  -2.00..-1.75s  (250 ms)
prev  -4.00..-3.70s  (300 ms)
```

-- which is the only thing that tells two overlapping spectra apart, and also
warns you when the two windows are different lengths. That matters: read such a
pair off the **density** panel, since power per bin would differ by the length
of the drag alone.

The console prints the band-by-band difference against the previous selection,
strongest first. The comparison
is made on **density** for the reason above:

```
   this selection vs the previous one (positive = stronger now):
        1334-1556    Hz    +14.3 dB
        1111-1334    Hz     +9.2 dB
```

That is the same question `imu_analyse.py` answers over a whole recording, but
on two windows you picked by eye -- useful for checking a hunch before
committing to a long capture.

**Selecting an artifact on the spectrogram is the main workflow.** Watch live;
when something interesting scrolls past, hit space and drag across it. The 8
second history means whatever you just noticed is still on screen, so there is
no need to pause pre-emptively. You get the FFT of exactly that time window, so
you can read the artifact's frequency content without the rest of the record
diluting it. Reset when done.

Selection is along time only, on both plots. The frequency axis is for reading,
and is changed by zooming (scroll on the spectrogram) rather than by selecting
-- so a selection never quietly restricts which frequencies the FFT covers.

Until the 8 second ring has filled, the time axis is wider than the history
behind it. Dragging in the empty part is refused with a message rather than
quietly handing back a handful of samples and a meaningless spectrum.

### Navigator and detail

The waveform is a **navigator** and the spectrogram is the **detail view**.

- The waveform always shows the whole 8 second ring. It never zooms.
- Selecting zooms the *spectrogram* to that window, and marks the window on the
  waveform as a cyan highlight.
- The selection overlay is removed from the spectrogram once applied, so the
  zoomed image is clean to read.
- That highlight is live: drag its middle to slide the window along, drag an
  edge to grow or shrink it. The spectrogram and the FFT follow.

So the two plots are deliberately *not* locked together once you are zoomed --
the whole point is to keep the overview while the detail view is somewhere
specific. They do still measure x in the same units (seconds before now), and
their plot boxes are aligned to the pixel, so at full view a feature in one
reads straight down into the other. The colourbar sits in its own column rather
than being attached to the spectrogram, which would otherwise take the width out
of that axes alone and leave the two time axes visibly out of step.

`r` (or **Reset view**) puts the spectrogram back to the full ring and clears
the highlight. Resuming does the same automatically: a window is defined in
seconds *before now*, so once data moves again it no longer points at what you
selected.

Zoom persists across live updates -- new data scrolling in will not throw the
view away. Zooming the *frequency* axis is useful live; zooming *time* is
usually better paused, since otherwise the window you zoomed into keeps
scrolling leftwards out of view.

Scrolling on the waveform zooms amplitude only -- its time span is fixed, so
there is always somewhere to drag the window to. Once you set the amplitude
axis by hand it stops auto-scaling, so a large transient will not rescale the
view out from under you; `r` gives it back.

The selection FFT runs over the whole selection as a single
transform at full sample resolution -- no sub-window averaging, because
averaging a millisecond transient against the background either side of it is
exactly what selecting it was meant to avoid. The title reports duration, peak
frequency, peak level, RMS and the resulting bin width.

Pausing freezes only the display; the serial link keeps draining, so the stream
never stalls and no frames are lost while you look.

The waveform is drawn as a min/max envelope, not decimated by dropping samples,
so a 1-sample spike is still visible at 8 seconds on screen. Selections index
the full-rate buffer underneath, never the drawn envelope.

Gravity is removed per FFT window, so both the colour scale and the spectrum
belong to the vibration rather than to DC. The title line reports achieved
sample rate and any dropped or corrupt frames -- watch it, because a silently
degraded link looks like a quiet sensor.

A caveat when reading peak levels: a tone landing between bins reads up to
1.4dB low with a Hann window (verified -- it is exactly the theoretical
scalloping loss). Fine for choosing bands, not for absolute calibration. Widen
the selection to narrow the bins.

`--axis mag` (the default) uses |a|, which catches the transient whatever
direction it arrives from. Use a single axis once you know the geometry.

### Colour scale

The colour axis is **fixed**, not auto-scaled: -55dB to +10dB re 1 m/s^2. That
is deliberate. Under autoscaling the same actuation renders differently
depending on what else happens to be on screen, you cannot compare two runs by
eye, and colour stops meaning a level at all.

The defaults come from measurement on this hardware, not taste:

| level | dB | where it lands |
|---|---|---|
| sensor noise floor (median) | -50 | just off black |
| quiet background, p99 | -40 | dark purple |
| modest transient | -16 | clearly visible |
| desk knock | +6 | near the top, not clipped |

Move them with `--vmin` / `--vmax` -- and do move them once you know how hard an
actuation actually hits, since the point is to put *your* signal in the middle
of the range. `--autoscale` exists for a first look at an unknown signal; do not
use it when comparing recordings.

## Recording

```
python3 imu_scope.py --record run1.npz                        # live + record
python3 imu_scope.py --no-plot --record run1.npz --seconds 60 # headless
```

Record a run with **several actuations and some quiet in between** -- the
analysis characterises the events against the background of the same recording,
so it needs both. `s` in the live view saves just the selected window instead,
which is the quicker way to grab one clean actuation.

## Analysis

```
python3 imu_analyse.py run1.npz --show-events
```

Locates the transients, then compares their spectra against the quiet parts and
prints a per-band separation table, best band first. The margin is the event
median against the background **95th percentile**, not its mean: that makes the
number a usable detection margin rather than a flattering average.

Useful knobs:

- `--k` threshold in MADs above the envelope median (default 8). Lower it if
  events are missed, raise it if background is being marked.
- `--hp` corner for the detection envelope only (default 200Hz), to keep hand
  motion and gravity wander out of the event finder.
- `--bands` how finely to split 0..Nyquist (default 16).
- `--axis`, `--nfft`, `--hop` as for the scope.

Always run `--show-events` once and check it framed the actual actuations. The
event finder is deliberately crude -- its only job is to pick windows, and every
number in the table inherits whatever it marked.

## Sanity check

The chain was validated by injecting 1400Hz damped bursts into a real quiet
recording: 6 of 6 events found at the correct times, and the 1338-1561Hz band
ranked top at +18.2dB, with the adjacent bands behind it. If you change the
analysis, re-run that check rather than trusting a plausible-looking table.

## Notes

- Sample rate is **measured**, not taken from the devicetree. The part clocks
  ~6% under its nominal ODR, and the entire frequency axis hangs off this. The
  port is flushed and the first half second discarded before timing, because a
  freshly opened port reads ~10% low while it re-syncs. Repeats land within
  ~1%, which is well inside one FFT bin.
- Noise floor measured at rest: ~25mm/s^2 RMS per axis over the full band.
- The range is +/-16g. If an actuation clips you will see it as broadband
  splatter across the whole spectrogram; nothing here detects that for you.
