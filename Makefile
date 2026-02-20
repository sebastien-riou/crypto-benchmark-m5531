
######################################
# target
######################################
TARGET = crypto-benchmark-m5531


######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og


#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

######################################
# source
######################################
# C sources

OUR_SRC = crypto-benchmark-m5531
NUVOTON_SRC = packs/Nuvoton/NuMicroM55_DFP/3.1.4-rc.3/Library
ARM_SRC = packs/ARM/CMSIS/6.3.0/CMSIS/Core

DEVICE_DIR = $(OUR_SRC)/RTE/Device/M5531H2LJAE

C_SOURCES =  \
$(OUR_SRC)/main.c \
$(DEVICE_DIR)/startup_M5531.c \
$(DEVICE_DIR)/system_M5531.c \
$(NUVOTON_SRC)/StdDriver/src/retarget.c \
$(NUVOTON_SRC)/StdDriver/src/uart.c \
$(NUVOTON_SRC)/StdDriver/src/sys.c \
$(NUVOTON_SRC)/StdDriver/src/pdma.c \
$(NUVOTON_SRC)/StdDriver/src/lppdma.c \
$(NUVOTON_SRC)/StdDriver/src/clk.c \
$(NUVOTON_SRC)/CMSIS/Driver/Source/misc.c \
$(NUVOTON_SRC)/CMSIS/Driver/Source/drv_lppdma.c \
$(NUVOTON_SRC)/CMSIS/Driver/Source/drv_pdma.c 

# ASM sources
ASM_SOURCES =

# ASMM sources
ASMM_SOURCES =



#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
OBJDUMP = $(GCC_PATH)/$(PREFIX)objdump
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
OBJDUMP = $(PREFIX)objdump
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S
DISASM = $(OBJDUMP) -D --visualize-jumps 
 
#######################################
# CFLAGS
#######################################
# cpu
CPU = -mcpu=cortex-m55

# fpu
FPU = -mfpu=fpv5-sp-d16

# float-abi
FLOAT-ABI = -mfloat-abi=softfp

# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# macros for gcc
# AS defines
AS_DEFS = 

# C defines
C_DEFS =  


# AS includes
AS_INCLUDES = 

# C includes
C_INCLUDES =  \
-I$(OUR_SRC)/RTE/_Debug_M5531H2LJAE \
-I$(DEVICE_DIR) \
-I$(NUVOTON_SRC)/Device/Nuvoton/M5531/Include \
-I$(NUVOTON_SRC)/StdDriver/inc \
-I$(ARM_SRC)/Include

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS += -std=gnu2x $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections -Wno-format

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-5
endif


# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"


#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = $(DEVICE_DIR)/M5531.ld
STACK_SIZE = 0x10000

# libraries
LIBS = -lc -lm -lnosys -lcrypto-benchmark
LIBDIR = -L../crypto-benchmark/build/cortex-m55/
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,--defsym=STACK_SIZE=$(STACK_SIZE) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).elf.asm $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin


#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASMM_SOURCES:.S=.o)))
vpath %.S $(sort $(dir $(ASMM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -save-temps -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.S Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.elf.asm: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(DISASM) $< > $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@
	
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	mkdir $@		

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)
  
#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***
