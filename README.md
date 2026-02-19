# Crypto-benchmark-m5531
Integration of crypto-benchmark on NuMaker-M5531 board

## Dependencies

### Cortex-M55 Toolchain
This projected as been tested with https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v14.2.1-1.1 on Ubuntu 24.04.

### CMSIS-Toolbox
https://open-cmsis-pack.github.io/cmsis-toolbox

### Other repositories
Install and build them using the initial setup script:
````
export CRYPTO_BENCHMARK_USE_GIT_SSH=1
./initial-setup
````

Note:
- the script build libraries also fr risc-v, so it requires a `riscv-none-elf-gcc` in the path. if you do not want that, comment out riscv builds in the build-all-target scripts. 
- the definition of `CRYPTO_BENCHMARK_USE_GIT_SSH` is needed only because `crypto-benchmark` repo is currently private. Once it is public, this will not be needed anymore.

## How to build and run using CLI
Build benchmark lib, for example:
````
cd ../crypto-benchmark
./buildit on/cortex-m55 mldsa 44
cd ../crypto-benchmark-m5531
````

Build the firmware using make:
````
./buildit
````

To load in flash and run:
````
./flash
````

To debug, use VSCode debug target 'launch'.

WARNING: right now, the only way to have timing measurement working is to run it under debug.
