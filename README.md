# physics

A multi-body rigid-body physics engine that runs **in your terminal**. Real-time numerical integration, body-body collision, mouse-to-spawn interaction, live performance stats, and a headless benchmark mode — all in ~1,000 lines of dependency-free C11.

```
make
./physics
```

`q` quits. Click anywhere in the play area to spawn a ball (up to 16 bodies).

## Why

Most portfolio work shown to recruiters is a web CRUD app or a copy-pasted ML notebook — neither signals the memory-aware, latency-aware, math-aware engineering that systems and infrastructure teams hire for. A small TUI physics engine — running a real-time integrator, redrawing a terminal at 60 fps with a diffed framebuffer, resolving pairwise collisions, parsing raw mouse escape sequences, and reporting hard throughput numbers — is dense evidence of that skill in a form a reviewer can clone and run in under a minute.

## Scenes

```
./physics --scene cradle      # Newton's cradle — momentum transfer down a line of balls
./physics --scene diagonal    # one ball bouncing forever, no gravity (energy-conservation check)
./physics --scene stack       # a column of balls falling and settling
./physics --scene default     # the original single bouncing ball
./physics --help              # list all options and scenes
```

Scenes are compile-time C functions in `scenes.c` — a scene is just "place N bodies with these initial states," which a function literal does in 5–15 lines. No runtime DSL, no config files.

## Tweak the physics

```
./physics --gravity 200       # heavier
./physics --gravity -40       # falls up
./physics --restitution 1.0   # perfectly elastic — energy is conserved
./physics --restitution 0.6   # lossy bounces
```

## Benchmark

Headless throughput measurement — no terminal init, no rendering, 1000-step warm-up, `clock_gettime` bookends:

```
$ ./physics --bench 1000000
scene: default, bodies: 1
1000000 steps in 0.009 s
~117M steps/sec
0.01 us/step
```

Numbers from a `make` release build (`-O2`) on Apple Silicon. The `make test` build adds AddressSanitizer + UndefinedBehaviorSanitizer and runs ~10× slower — use it for verification, not benchmarks.

## How it works

The core loop is **simulate → render → pace → interact**, with five single-responsibility modules in one process, one thread, no async, and no allocations on the hot path.

### Physics — `physics.c`
- **Semi-implicit (symplectic) Euler** integrator at a fixed `dt = 1/240 s`. Symplectic because it keeps energy *bounded* over long runs — a naive explicit Euler integrator visibly gains energy and balls climb the walls.
- **Body-body collision** is a pairwise O(N²) elastic-impulse resolve using the closed-form 2D normal-component swap, scaled by combined restitution. At `MAX_BODIES = 16` that's ≤120 pair checks per step — trivial; spatial hashing or sweep-and-prune would be bookkeeping that obscures the intent.
- **Collision ordering**: walls first (infinite mass), then body-body, within one `world_step`. Resolving wall penetration first means body-body resolution never sees a body clipped through a wall.

### Rendering — `render.c`
- **Unicode Braille** (`U+2800`–`U+28FF`): each character cell packs a 2×4 dot grid, giving **8× the effective resolution** of one glyph per body.
- **Per-cell frame diff**: the renderer keeps front/back framebuffers and emits only the cells that changed, run-coalesced into cursor-move + write bursts. An idle frame costs **zero bytes on the wire**.
- **Stats overlay**: row 1 shows FPS, body count, and total kinetic energy at 4 Hz, drawn as plain ASCII outside the Braille diff path (~10 bytes/frame, no diff bookkeeping).

### Terminal driver — `term.c`, `term_parse.c`
- Raw `termios`, alternate screen, hidden cursor, ANSI escape codes — **no ncurses, no termbox**.
- **SGR mouse mode** (`\x1b[?1006h`), parsed by a small state machine. SGR uses decimal coordinates so it works past column 223 (where the legacy X10 fixed-byte protocol breaks) and is supported across iTerm2, Terminal.app, Alacritty, WezTerm, Kitty, xterm, and gnome-terminal.
- Signal-safe shutdown: `SIGINT`/`SIGTERM`/`SIGHUP`/`SIGWINCH` set flags polled by the main loop; an `atexit` handler always restores the terminal.

### Main loop — `main.c`
- **Fixed-timestep accumulator** (Fiedler's "Fix Your Timestep!"): physics steps at a deterministic 240 Hz while rendering targets 60 fps, so simulation behavior is independent of frame rate.
- Owns arg parsing, scene dispatch, mouse-spawn handling, and the `--bench` path (which skips terminal/render init entirely for a clean measurement).

**Memory model:** bodies live in a fixed-size static array; the only heap allocations are the two Braille framebuffers, sized to the terminal at startup and reallocated on `SIGWINCH`. The output buffer is a static 256 KiB array. Nothing allocates on the per-frame path.

## Design decisions

| Decision | Choice | Why |
|---|---|---|
| Language / build | C11, `make` + GCC/Clang, zero deps | Classical-systems signal; one command to a binary. |
| Compiler flags | `-Wall -Wextra -Wpedantic -Werror`; ASan/UBSan in dev, `-O2` release | Sanitizers catch UAF/OOB before they corrupt the terminal mid-demo. |
| Integrator | Symplectic Euler | Energy stays bounded; correct-looking long-run behavior. |
| Rendering | Braille 2×4 dot grid + frame diff | 8× cell density; idle frames cost nothing. |
| Collision | Pairwise O(N²) impulse | ≤120 pairs at N=16; closed-form and legible beats a spatial structure here. |
| Mouse | SGR (`?1006h`) | Modern default; works past column 223 across every common terminal. |
| Scenes | Compile-time function table | A scene is a 10-line C function, not a separate program. |
| Threading | Single-threaded | No shared state, no races, nothing to synchronize. |

## Requirements

- C11 compiler (GCC or Clang)
- POSIX terminal (macOS or Linux)
- A monospace font with Braille glyphs — JetBrains Mono, Iosevka, Menlo, or Cascadia Code

## Build & test

```
make           # release binary (-O2)
make test      # 53 unit tests with ASan + UBSan
make clean
```

The test suite is a hand-rolled runner (no Unity, no Check) covering the physics integrator and collisions, the renderer, scene initializers, and the SGR mouse parser — 53 tests, the terminal I/O side verified manually.

## Layout

```
physics.c / physics.h   # integrator, wall + body-body collision, kinetic energy
render.c / render.h      # Braille framebuffer, rasterization, per-cell frame diff, stats line
term.c / term.h          # termios raw mode, escape codes, signals, SGR mouse
term_parse.c             # SGR mouse-sequence state machine
scenes.c / scenes.h      # default, cradle, diagonal, stack initializers
main.c                   # accumulator loop, arg/scene dispatch, bench mode
test/                    # hand-rolled test runner + suites
```

C11, ~1,000 LOC, no dependencies.

## References

- Glenn Fiedler, *Fix Your Timestep!* (2004) — the accumulator pattern.
- Erin Catto, *Iterative Dynamics with Temporal Coherence* (2005) — why pairwise O(N²) is fine at small N.
- XTerm Control Sequences — SGR mouse mode (`?1006h`).
