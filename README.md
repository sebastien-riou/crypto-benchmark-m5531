# Crypto-benchmark-m5531
Integration of crypto-benchmark on NuMaker-M5531 board

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
Install and build them using the initial setup script:
````
./initial-setup
````

Note:
- the script build libraries also for risc-v, so it requires a `riscv-none-elf-gcc` in the path. if you do not want that, comment out riscv builds in the build-all-target scripts. 

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
