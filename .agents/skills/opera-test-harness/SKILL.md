---
name: opera-test-harness
description: Use when running, automating, or debugging the 3DO libretro "opera" core with the test-harness binary, including terminal mode, controls, BIOS/core defaults, screenshots, ROMs, scripted input, and benchmarks.
---

# opera-test-harness

A headless runner for the `opera_libretro.so` 3DO core. It loads a
libretro core + 3DO BIOS + (optional) disc image, runs frames with no
display/audio hardware needed, and emits screenshots, audio, and
structured metrics for CI or regression use.

## When to use this skill

Trigger when:

- User asks to "run the core", "test the core", "boot a 3DO disc",
  "capture a screenshot" of a 3DO game, or "benchmark" the core.
- User mentions `test-harness`, `test_harness.c`, "test harness" or
  headless core testing.
- User asks to script button presses or compare runs against a
  baseline.

## Build

From the repo root:

```sh
make harness    # builds ./test-harness
```

The harness always compiles the vendored zlib encoder used by Kitty
graphics. It also depends on `tools/stb_image_write.h` and links
`-ldl -lm`.

## BIOS

If `--core` is omitted, the harness looks for `opera_libretro.so`
beside the `test-harness` executable. If `--bios` is omitted, it
prefers `panafz1.bin` beside `test-harness`, then falls back to
filename search.

`--bios` accepts either a real path or a bare filename listed in
`BIOS_ROMS[]` (`tools/test_harness.c`). Bare filenames are searched
beside `test-harness`, then in RetroArch system dirs
(`RETROARCH_ROM_DIRS[]`). Use `--list-bios` to print recognized BIOS
ROMs and any resolved local paths.

## Canonical invocation

```sh
./test-harness \
  --core ./opera_libretro.so \
  --bios /path/to/panafz1.bin \
  --title "/path/to/Game (USA).bin" \
  --frames 600 \
  --screenshot-at 120=/tmp/early.png \
  --screenshot-at 300=/tmp/title.png \
  --screenshot /tmp/final.png \
  --wall-timeout 30 \
  --output-dir /tmp/run
```

`--core` and `--bios` are optional when the defaults are available
beside `test-harness` or in the configured BIOS search
paths. Everything else is optional.

## Key flags

### Duration
- `--frames N` — exact frame count.
- `--seconds S` — `ceil(S * core_fps)` frames (60 fps for NTSC).
- `--wall-timeout S` — abort if the run wallclock exceeds S seconds;
 watchdog against a hung core. **Always set this** for CI/automated
 runs.

### Capturing frames
- `--screenshot PATH` — final frame as PNG.
- `--screenshot-at N=PATH` (repeatable) — specific frame N as PNG.
- `--screenshot-every N=DIR` — every Nth frame as
 `DIR/frameNNNNNN.png`.  Useful for contact-sheet previews and
 boot-animation capture.
- `--screenshot-when MODE` — filter `--screenshot-every` output:
  `NONBLANK` skips all-black frames; `CHANGED` skips frames identical
  to the previous one written. Reduces contact-sheet noise from
  frozen/loading screens.

### Input
- `--input SPEC` (repeatable) — `WHEN=BUTTON`, optionally
 `WHEN+DURATION=BUTTON`.
- `--input-file PATH` — text file with one `SPEC` per line; blank
 lines and `#`-prefixed comments are skipped.

Spec syntax:
- `WHEN` = `NF` (N frames) or `NS` (N seconds, converted via core
  fps).
- `DURATION` = same units; defaults to 1 frame if omitted.
- `BUTTON` ∈ `A B C X P L R UP DOWN LEFT RIGHT` (case-insensitive).

Examples:
- `120F=A` — press A on frame 120 only.
- `2S=X` — press X at 2 s for 1 frame.
- `2S+0.5S=A` — press A at 2 s, hold for half a second.
- `120F+30F=P` — press P at frame 120, hold for 30 frames.

### Audio & artifacts
- `--audio PATH` — stereo s16le WAV of the whole run.
- `--log PATH` — per-run log (default `<output-dir>/run.log`).
- `--metrics PATH` — JSON with frame counts, fps, timings, input
 events, screenshot tallies, etc. (default
 `<output-dir>/metrics.json`).

### Core options
- `--option KEY=VALUE` (repeatable) — override core options, e.g.
 `--option opera_region=ntsc`, `--option
 opera_high_resolution=enabled`.  Keys are normalized
 (case-insensitive, `_`/`-` interchange).

### Deterministic PRNG configuration

- `--option opera_random_seed=VALUE` selects the initial seed for both
  emulated random-number generators.  Use a fixed value when reproducing a
  run; the default `random` instead uses the current time during core
  initialization.
- Accepted fixed presets are `0x00000000`, `0x00000001`, `0xdeadbeef`,
  `0xcafebabe`, `0xfeedface`, `0xbaadf00d`, `0x8badf00d`, `0xdeadc0de`,
  `0xc001d00d`, `0x0badf00d`, `0x74726170`, and `0x65786974`.  The core also
  accepts a decimal or `0x`-prefixed hexadecimal unsigned 32-bit value from
  a frontend that exposes custom option values.
- The option is read only when the core initializes.  Fully reinitialize the
  core after changing it; do not expect `retro_reset` alone to reseed a run.
- Savestates version 3 and newer contain a `PRNG` chunk with both generator
  states, so loading a state resumes its random sequence independently of
  the seed option currently selected.

Example:
```sh
./test-harness --core ./opera_libretro.so --bios panafz1.bin \
  --title /path/to/game.cue --frames 600 --wall-timeout 30 \
  --option opera_random_seed=0xdeadbeef
```

### Terminal mode
- `--terminal` — opt-in live terminal framebuffer and keyboard
  controls.
- `--terminal-fps N` — cap terminal redraw rate; `0` redraws every
  video frame.  Default is adaptive up to 30 FPS.
- `--terminal-render auto|kitty|sixel|half|ascii` — override render
  mode. Default is capability-detected; `auto` probes Kitty graphics,
  then Sixel graphics, then falls back to half-block/ascii when
  unsupported.
- `--terminal-color auto|true|256|mono` — override color mode. Default
  is capability-detected.
- `--terminal-button-hold N` — keep terminal button presses active for
  N frames after a key byte is received. Default is 6.

Terminal controls:
- `W/A/S/D` = Up/Left/Down/Right.
- Arrow keys are secondary movement aliases.
- `J` = 3DO A.
- `K` = 3DO B.
- `L` = 3DO C.
- `'` = 3DO X/Stop.
- `U` = Left Trigger.
- `I` = Right Trigger.
- `Enter` = Play.
- `Esc Esc` = quit terminal mode.
- `Ctrl-C` also requests quit/cleanup.

Terminal implementation notes:
- Terminal mode opens `/dev/tty`, uses raw mode, alternate screen, and
  cursor hide/show sequences.
- Rendering uses Kitty graphics when probed and supported, then Sixel
  graphics when supported, half-block Unicode `▀` when supported, and
  ASCII fallback when needed. Forced image renderers warn and fall back
  if their probe fails. Auto rendering uses ASCII without image probes
  when terminal color mode is mono or `NO_COLOR` is set.
- Kitty performs a second capability probe for zlib-compressed raw RGB.
  Supported terminals receive `f=24,o=z` payloads; if that probe or a
  frame compression fails, rendering falls back to PNG payloads.
- Native Sixel is disabled inside tmux because tmux can leave the final
  image in the outer terminal after the pane restores. Run outside tmux
  for Sixel, or use the half/ascii fallback inside tmux.
- On exit, Kitty images are deleted by image id. Sixel output clears the
  alternate screen before restore, then clears the restored normal screen
  for terminals that keep Sixel graphics outside the alternate buffer.
- Image renderers track emitted image bytes, render timings, and
  skipped unchanged frames in `metrics.json`. Kitty compression support
  is reported as `terminal_kitty_zlib_probed`,
  `terminal_kitty_zlib_supported`, and
  `terminal_kitty_zlib_probe_result`.
- Image renderers use an exact active-row frame cache to skip duplicates;
  Sixel also caches scale maps and palette lookup tables. Text renderers
  use row hashes, row diffing, changed-span emission, batched writes, and
  run-length glyph output.
- `SIGWINCH` marks terminal size dirty; size is refreshed on
  open/resize rather than every rendered frame.
- Termination signals exit through normal cleanup so raw mode and
  alt-screen are restored.

## Exit codes / status

- `0` — `ok`: ran cleanly.
- `1` — `error` / `artifact_error` / `metrics_error`: core failed to
 run, a screenshot/audio/metrics write failed, or required args
 missing.
- `124` — `timeout`: `--wall-timeout` fired.
- `128+signal` — terminated by a handled process signal after cleanup.

The terse stderr line is `test-harness: <status>, frames=N, log=...,
metrics=...`.  Full detail lives in `metrics.json` under `status`,
`exit_code`, `frames_run`, `average_fps`, `speed_multiplier`,
`input_events[]`, `log_counts`, etc.

## Common workflows

### Boot-to-title screenshot
```sh
./test-harness --core ./opera_libretro.so --bios panafz1.bin \
  --title "/path/to/Game.cue" --seconds 10 \
  --screenshot /tmp/title.png --wall-timeout 30
```

### Scripted playthrough
Write a `inputs.txt` with one event per line and comments:
```
#dismiss BIOS + publisher logos
20S=A
22S=A
24S=A
#enter main menu, start game
30S=A
30S+2S=X        # hold X through the load screen
```
Then:
```sh
./test-harness --core ... --bios ... --title ... \
  --frames 2400 --input-file inputs.txt \
  --screenshot-every 300=/tmp/frames --wall-timeout 60
```

### Multiple capture points plus contact sheet
```sh
./test-harness --core ... --bios ... --title ... \
  --frames 600 \
  --screenshot-at 60=/tmp/01.png \
  --screenshot-at 600=/tmp/end.png \
  --screenshot-every 120=/tmp/grid
```

### Regression / benchmarking
Pin a stable frame window so rebuilds can be compared apples-to-apples:
```sh
./test-harness --core ... --bios ... --title ... \
  --frames 1800 \
  --benchmark-start-frame 600 --benchmark-end-frame 1800 \
  --cpu 2 --wall-timeout 120 \
  --metrics /tmp/metrics.json
```
`metrics.json` `benchmark_average_fps` and `benchmark_speed_multiplier` are
the stable comparison numbers.

## Gotchas

- **`--core` and `--bios` are required.** `--bios` may be a bare
  filename only
- **`--core` and `--bios` have executable-directory defaults.** If the
  defaults are not available, pass explicit paths or put recognized
  BIOS filenames in a searched RetroArch system directory.
- **`--wall-timeout` counts cumulative wall seconds, not per-frame.**
 For hung-core detection in CI set it generously above expected wall
 time.
- **Screenshots are written via `stb_image_write`** (PNG, no external
 deps).  `--screenshot-at` captures are buffered in RAM until
 end-of-run; `--screenshot-every` PNGs are written immediately, so
 memory stays flat even for very long runs.
- **`--screenshot-when` compares pixels (not padding).** It packs each
 frame by `width * pixel_size` before comparing, so stride/padding
 differences don't cause false "changed" results. `CHANGED` keeps a
 packed copy of the last written frame in memory (~width*height*4
 bytes max).
- **Input ports.** Input events target controller port 0 (joypad)
 only. Multi- port play is not yet supported.
- **Frame numbering is 1-based.** `--screenshot-at 1` captures the
  first frame.
- **GPU threads are pinned with `--cpu`,** which also sets affinity
 for any threads the core spawns.
- **Run artifacts go under `--output-dir`** (default
 `./test-harness-runs/<YYYYMMDD-HHMMSS-pid>/`); the temp `work/`
 subdir (system + save) is removed unless `--keep-work-dir` or
 `--work-dir` is set.
