# physics

A multi-body physics playground in your terminal. Real-time rigid-body simulation with Newton's cradle, mouse-to-spawn, headless benchmark mode — all in plain C, no dependencies.

```
make
./physics
```

`q` quits. Click anywhere in the play area to spawn a ball (up to 16 bodies).

## Scenes

```
./physics --scene cradle      # Newton's cradle
./physics --scene diagonal    # one ball bouncing forever, no gravity
./physics --scene stack       # column of balls falling
./physics --help              # list all options and scenes
```

## Tweak the physics

```
./physics --gravity 200       # heavier
./physics --gravity -40       # falls up
./physics --restitution 1.0   # perfectly elastic
```

## Benchmark

Headless throughput measurement (no terminal, no rendering, 1000-step warm-up):

```
$ ./physics --bench 1000000
scene: default, bodies: 1
1000000 steps in 0.005 s
~200M steps/sec
0.01 us/step
```

Numbers from a `make` release build (`-O2`). The `make test` build adds AddressSanitizer + UndefinedBehaviorSanitizer and runs ~10× slower — use it for verification, not benchmarks.

## How it works

- **Physics**: semi-implicit (symplectic) Euler integrator at 240 Hz. Body-body collision via pairwise elastic-impulse with combined restitution. Walls via axis-clamp + restitution.
- **Rendering**: Unicode Braille (8 dots per cell, 8× effective resolution). Per-cell frame diff with run-coalesced output — idle frames cost zero bytes on the wire.
- **Terminal**: raw `termios`, ANSI escape codes, SGR mouse mode. No `ncurses`, no termbox.
- **Pacing**: accumulator pattern decouples physics (deterministic) from rendering (60 fps target). Single-threaded; no allocations on the hot path.
- **Stats overlay**: top row of the terminal shows FPS, body count, and total kinetic energy at 4 Hz.

C11, ~1000 LOC, no dependencies.

## Requirements

- C11 compiler (GCC or Clang)
- POSIX terminal (macOS or Linux)
- Monospace font with Braille glyphs — JetBrains Mono, Iosevka, Menlo, or Cascadia Code

## Build

```
make           # release binary (-O2)
make test      # unit tests with ASan + UBSan
make clean
```
