# Galvo Scanner Firmware Review — Timing & FLIM LABS Card Compatibility

Review of `main/src/scanner/` (`HighSpeedScannerCore`, `GalvoController`,
`DAC_MCP4822`) against what the FLIM LABS acquisition card (flim-imager
server) needs. July 2026.

## How the card consumes the scan

The card counts photons continuously and reconstructs the image from marker
pulses on its sync inputs. Its `reconstruction` mode declares which markers
are wired: `PLF` (pixel+line+frame), `LF` (line+frame — pixels sliced by
`dwell_time`), `F` (frame only — lines *and* pixels sliced by `dwell_time`).
The firmware's three trigger pins (`galvo_trig_pixel/line/frame`, GPIO 2/3/4
on the Xiao galvo build) map exactly onto this — good foundation.

## Findings

### F1 — Trigger pulses are ~25–50 ns; `trig_width_us` is ignored (**most important**)

In the raster loop (`HighSpeedScannerCore.cpp` ~lines 553–584) every pulse is
a back-to-back `GPIO.out_w1ts` / `out_w1tc` register write pair → roughly
25–50 ns high time. The configured `trig_width_us` (and `trig_delay_us`) are
never read in the raster path; the 5 µs `triggerPulseLine()` /
`triggerPulseFrame()` helpers are dead code there. Whether the card's input
stage registers 25 ns pulses is unverified — scan heads it normally ingests
produce µs-scale TTLs, and there is cabling/EMI margin to worry about.

**Adjustment:** honor `trig_width_us` for line/frame pulses
(`set mask → esp_rom_delay_us(width) → clear mask`). For the pixel clock keep
the width well below the dwell time (e.g. min(trig_width_us, dwell/4)), or
verify with FLIM LABS the minimum detectable pulse width and edge polarity.
An oscilloscope check on all three pins before first light is strongly
recommended.

### F2 — Frame, line and pixel edges are exactly coincident

At the first imaging sample of line 0 all three pins rise in a single
register write. If the card's FPGA expects frame-start to *precede* the first
line marker (sequential state machine), a coincident edge may drop the first
line or frame. **Adjustment (if needed):** fire the frame pulse during the
pre-blanking region (`pre_samples` ≥ a few samples gives a clean lead time),
or confirm with FLIM LABS that coincident edges are accepted.

### F3 — Minimum dwell is SPI-bound: ~2 µs, and the default (1 µs) always overruns

Each X update = 16-bit MCP4822 word at 20 MHz SPI (0.8 µs) + CS/polling
overhead + LDAC pulse ≈ 1.5–2 µs. With the default `sample_period_us = 1` the
busy-wait pacing loop can never keep up: the scan runs SPI-paced (~2 µs
effective dwell) with jitter, and `timing_overruns` counts up.

- In **PLF** wiring this is benign for geometry (the pixel clock tells the
  card the truth) but dwell varies per pixel → intensity/lifetime statistics
  per pixel vary.
- In **LF/F** wiring the card slices by nominal dwell → accumulated jitter
  shears the image within a line (LF) or across the frame (F).

**Adjustment:** treat ≥ 3–5 µs as the practical dwell floor, surface
`timing_overruns` in ImSwitch, and prefer PLF wiring when pulse detection
(F1) is confirmed. Long-term: DMA/ledc-based pixel clock generation would
decouple the clock from the SPI loop.

### F4 — Bidirectional mode misaligns the trigger window (use unidirectional for FLIM)

`reverse_line` reverses the *entire* line profile (pre + ramp + flyback +
settle), but the trigger window stays at sample indices
`[pre_samples, pre_samples + nx)`. In a reversed line the linear ramp
actually occupies `[fly_samples + settle, fly_samples + settle + nx)`, so
markers fire partly during flyback/settle unless
`pre_samples == fly_samples + line_settle_samples`. The FLIM card also
assumes monotonic line order. **Adjustment:** keep `bidirectional = 0` for
FLIM (default); if bidi is ever needed, mirror the trigger window for
reversed lines and let the card/bridge reorder pixels.

### F5 — `enable_trigger` config is ignored

`bool do_trig = 1; //(config_.enable_trigger != 0)` hardcodes triggers on.
Harmless for FLIM, but the API silently lies. One-line fix.

### F6 — Marker leads the DAC move by ~1–2 µs (constant, acceptable)

The pixel marker fires at the pacing instant; the SPI write moving the beam
to that pixel completes ~1–2 µs later. This is a *constant* pixel-position
offset along X — absorbed by the card's `offset_left` / affine calibration.
No change needed; just calibrate with the real signal chain.

### F7 — Flyback photons (LF/F modes need overscan modelling)

Markers only fire during the imaging window, but the card counts photons
continuously. In LF/F mode set FLIM `scan_width = nx + fly_samples +
line_settle_samples + pre_samples` (i.e. `image_width` + `offset_right` =
full marker-to-marker period) so flyback photons land in discarded overscan
pixels instead of smearing the image edge. The ImSwitch FLIM bridge panel
does this automatically. Alternatively gate the laser during flyback via the
`galvo_laser` pin (not currently done in raster mode).

## Summary

| # | Severity for FLIM | Change needed in firmware? |
|---|---|---|
| F1 pulse width | High (may not trigger at all) | Yes — honor `trig_width_us`, scope-verify |
| F2 coincident edges | Medium (verify with FLIM LABS) | Maybe — pre-fire frame in pre-blanking |
| F3 dwell floor ~2 µs | Medium | No (document + use ≥3–5 µs); DMA clock later |
| F4 bidirectional window | Low (default off) | Only if bidi needed |
| F5 enable_trigger ignored | Cosmetic | One-liner |
| F6 marker-vs-DAC skew | None | No — calibrate |
| F7 flyback overscan | Handled in bridge | Optional laser gating |

The trigger architecture (3 dedicated pins, frame/line/pixel) is exactly what
the card needs; the work items are pulse width (F1), edge ordering
verification (F2), and respecting the dwell floor (F3).

## Implementation status (July 2026)

All items above are now addressed in `HighSpeedScannerCore` /
`GalvoController` (new `ScanConfig` fields: `overscan_samples`,
`laser_blanking`, `hw_pixel_clock`; plumbed through the JSON API, UC2-REST
`galvo.set_galvo_scan`, the ImSwitch manager/REST API and the React UI):

- **F1/F2**: `trig_width_us` is honoured for all markers; frame+line markers
  fire at line start (during pre-blanking), `trig_delay_us` spaces frame →
  line. Pixel pulse width is capped at half the dwell.
- **F3**: optional hardware pixel clock (`hw_pixel_clock=1`) via the RMT
  peripheral — exactly nx equidistant pulses per line, decoupled from the
  DAC/SPI loop (ESP32-S3; software fallback elsewhere).
- **F4**: bidirectional now uses a mirrored line profile with the identical
  trigger window — monotonic, equidistant pixel counts in both directions
  (the host flips odd lines when reassembling).
- **F5**: `enable_trigger` is respected.
- **Flyback/lag**: `overscan_samples` extends the ramp at constant pixel
  slope on both sides of the imaging window, so the mirror is at constant
  velocity when triggers/laser start. Keep x_min/x_max inside the DAC range
  by at least `overscan_samples × slope` counts, or the ramp clips.
- **F7**: `laser_blanking=1` gates the `galvo_laser` pin HIGH only during the
  imaging window. The ImSwitch FLIM bridge models the remaining overscan as
  `offset_left = pre + overscan`, `offset_right = overscan + fly + settle`.

Still to verify on hardware: scope all three marker pins, confirm the card's
minimum pulse width and coincident-edge behaviour, and measure the RMT-vs-DAC
alignment at the line start.
