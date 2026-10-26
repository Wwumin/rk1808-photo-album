# Building a boardless harness for an embedded GUI

This repository is one worked example. The reusable part is the *shape* of the harness:
three seams you cut, then a script that drives the real program. This page is the
extracted recipe — short enough to adapt to another framebuffer/touch project in an
afternoon.

## 1. Decide whether it pays off

A boardless harness pays off when most of your iteration cost is *getting pixels onto a
screen*, and when your bugs are state-machine bugs rather than electrical ones. It does
**not** replace the board for:

- pixel-format mistakes (`RGB565` vs `ARGB8888` byte order),
- tearing / vblank behaviour,
- panel power-up and vendor init sequences,
- real touch calibration,
- anything that depends on temperature, power or timing hardware.

The point is not "simulate the hardware". The point is: **run the whole program on every
push, on the same source that ships**. The harness in this repo runs 46 scripted touches
through the actual `main()` and exits non-zero on any engine failure.

## 2. Cut three seams

Only three interfaces need substitutes. Keep them tiny and keep the substitutes in
`test/`, never in `album/`.

| Seam | Production | Harness | Notes |
|---|---|---|---|
| Framebuffer | `DRMshowUp()` blits to the DRM device | `test/stub/host/drm_stub.c` paints into a `malloc`ed 1024×600 BGRA buffer and can dump 24-bit BMPs | also stub `DRMinit`, `DRMcreateFB`, `DRMfreeResources` |
| Input | `/dev/input/event*` touch events | `test/stub/host/touch_stub.c` replays a script of `x y hold_ms` lines | press/release are generated as tick-based phases |
| Vendor headers | `inc/DRMwrap.h` + `lib/libDRMwrap.so` | `test/stub/inc/DRMwrap.h` (same name, earlier `-I`) | shadowing means no `#ifdef` pollution in app code |

The header shadow is the trick that keeps the application code untouched: the compiler
sees a different `DRMwrap.h` when `-Istub` comes before the vendor include directory.

## 3. Drive it with a script, not a test framework

`test/clicks.txt` is plain text, one gesture per line:

```
# <x> <y> <hold_ms>
512 300 100     # enter album
64  561 100     # slideshow
900 561 100     # options
```

The stub feeds these to the app as touch events, a few frames apart, then calls
`g_script_done_quit = 1` so the program exits by itself. Exit code 0 means the state
machine made it to the end — including the modules the script entered.

Why a script and not assertions on pixels: it survives layout changes, produces a human
readable audit trail in the log, and encodes *intent* ("tap the options button") rather
than pixel values. Frame dumps are still exported every 20 frames for visual spot checks.

## 4. Make it a CI gate

```yaml
- name: Unit tests           # milliseconds: pure logic
  run: make -C test unit
- name: Regenerate assets    # the art is code, so CI regenerates it
  run: make assets PYTHON=python3
- name: Smoke test (pinned seed, reproducible)
  run: make -C test run
- name: Smoke test (seed changes every run)
  env: { ALBUM_SEED: "${{ github.run_number }}${{ github.run_attempt }}" }
  run: make -C test run
```

Two rules that came from real failures:

1. **Seed the randomness.** Every `rand()` goes through one seedable entry point, so a
   red CI run can be replayed locally (`ALBUM_SEED=… make -C test run`).
2. **Also run an unseeded variant.** A pinned seed keeps failures reproducible, but it
   only ever tests one layout. The minesweeper stack overflow in v1.0.2 was hit only on
   ~2 % of boards: unseeded CI found it, local green runs never could.

## 5. What the harness caught that review did not

- 32-bit assumption in the bitmask test (`N_ABS >= 2`), invisible on MinGW, fatal on
  64-bit Linux.
- Hardcoded `C:\Windows\Fonts\...` in the asset pipeline — caught by building on a clean
  Linux runner.
- A stack overflow in minesweeper's flood fill (~2 % of layouts), caught by the Linux
  stack canary that MinGW does not have.

None of those are hardware bugs. That is the argument for a harness: your board is a
slow, single-use CI machine; a hostile-but-fast one finds the same defects earlier.

## 6. Cost

Two days to build the stubs for this project, and the whole check runs in about a minute.
The harness is 3 stub files (~13 KB of C) plus 46 scripted gestures. It has paid for
itself several times over.
