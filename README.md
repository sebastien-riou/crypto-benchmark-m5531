# Crypto-benchmark-m5531

> [!WARNING]
> **Deprecated.** This repository is no longer maintained. crypto-benchmark now runs on the NuMaker-M5531 board as a
> [lean-bsp](https://github.com/sebastien-riou/lean-bsp) application:
> [crypto-benchmark](https://github.com/sebastien-riou/crypto-benchmark)'s board glue built with
> [lean-bsp-numaker-m5531](https://github.com/sebastien-riou/lean-bsp-numaker-m5531), a board support package shared
> by every lean-bsp application. Follow crypto-benchmark's README, section
> ["Benchmarking on a board"](https://github.com/sebastien-riou/crypto-benchmark#benchmarking-on-a-board).
>
> Compared with this project, that build runs the same machine code for every benchmarked function, with the same
> outputs, stack, heap and footprint; average cycle counts moved by less than 1 %, from where the linker places the
> code. It also starts reliably, where this firmware sometimes hung before the host's handshake. This repository stays
> as it was, for reference.

Integration of crypto-benchmark on NuMaker-M5531 board

## Instead: the lean-bsp build

With `lean-bsp`, `lean-bsp-numaker-m5531` and [lean-hwbridge](https://github.com/sebastien-riou/lean-hwbridge) set up
as crypto-benchmark's README describes, from the `crypto-benchmark` directory, for example:

````
pipenv run python benchmark.py --hw-platform numaker-m5531 --lib pqm4 --algo mlkem --pset 768
````

The results carry the board's name in lean-bsp, `numaker-m5531`, where this repository's carried `m5531`. Since
lean-benchmark v0.0.12 the caches are cleared before each timed operation, which this firmware did not do, so cycle
counts are a little higher than this repository's.

The instructions below are this repository's own, kept as they were.

## Dependencies

### Cortex-M55 Toolchain
This projected as been tested with https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v14.2.1-1.1 on Ubuntu 24.04.

### CMSIS-Toolbox
https://open-cmsis-pack.github.io/cmsis-toolbox

### Pipenv
See [pipenv-howto.md](https://gist.github.com/sebastien-riou/49b2a054fb6c6cf98ec00315070ee0a5)

`pyocd` is installed by `pipenv install` into this repo's virtualenv, never system-wide. `./flash`
and `./run` locate it there themselves (via `pipenv --venv`), so they need no `pipenv run` prefix
and work the same from a plain terminal, from another script, or from the VSCode integrated
terminal -- the last of which auto-activates the virtualenv and so used to be the only place a bare
`pyocd` resolved. They also work from any working directory. If you see
`no pyocd in this repo's virtualenv`, run `pipenv install` here.

### Other repositories
This repo is set up as an add-on of [crypto-benchmark](https://github.com/sebastien-riou/crypto-benchmark)'s
`initial-setup`, which builds `crypto-benchmark`'s own library for `cortex-m55`,
runs `pipenv install` here (to bootstrap `pyocd`), and then builds this repo's
firmware:
````
cd ../crypto-benchmark
./initial-setup --level=custom --addons=crypto-benchmark-m5531
````

## How to build and run using CLI
Build benchmark lib, for example:
````
cd ../crypto-benchmark
python3 link_ext.py --goal=small
./buildit on/cortex-m55 mldsa 44
cd ../crypto-benchmark-m5531
````

Build the firmware using make:
````
./buildit
````

To load in flash, then start execution, then collect the results:
````
./flash
./run
UART=$(./find-uart)
(cd ../crypto-benchmark && ./get-results "$UART" --device-timeout=180)
````

`./find-uart` prints the board's VCOM path, so nothing has to hardcode `/dev/ttyACM0`. It resolves a
`/dev/serial/by-id` entry, whose name embeds the Nu-Link2-Me's USB serial number and is therefore
stable across reboots and enumeration order. With more than one board attached, pin the one you want
with `NULINK_UID=<probe serial>` -- that same variable also selects the probe for `./run`, since
pyocd matches probes on the same serial number. `M5531_UART=<path>` overrides the lookup entirely.
`./run` prints the resolved path after resetting, as a convenience.

**Run these three in this order.** The order is load-bearing, not cosmetic:

- `./flash` needs the pack (`--cbuild-run make.cbuild-run.yml`) for the flash algorithms, and the
  pack's `DebugPortStop` debug sequence ends with `WriteDP(DP_CTRL_STAT, 0x00000000)`, which powers
  the debug power domain down. The DWT and the PMU live in that domain, so afterwards `DWT->CYCCNT`
  is frozen and writes to `DWT->CTRL` / `PMU->CTRL` are silently dropped. Firmware cannot undo it:
  those bits are in the debug port, reachable only over SWD, never from the core.
- `./run` resets the board with the generic `cortex_m` target and no pack, so it loads no debug
  sequences: it powers the debug domain back up on connect and leaves it up. It exits immediately,
  with the board running.
- `get-results` then drives the lean-com handshake. The board waits in that handshake at startup, so
  it can be started at any time after `./run` returns.

Skipping `./run` leaves the cycle counter dead. The firmware self-checks for that and prints
`WARNING: DWT CYCCNT is not counting, all timing measurements will read 0`, and checks again after
the run in case a debugger detached partway through -- so a dead counter is reported rather than
quietly turning every measurement into a constant.

To debug, use VSCode debug target 'launch'. Timing measurements no longer require a debug session:
`dwt_enable()` in `main.c` sets `DEMCR.TRCENA` itself instead of relying on the debugger to set it.
