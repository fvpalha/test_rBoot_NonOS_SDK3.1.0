#
# Makefile for rBoot sample project
# https://github.com/raburton/esp8266
#

# use wifi settings from environment or hard code them here
WIFI_SSID ?= ""
WIFI_PWD  ?= ""

# spiffs configuration
SPIFFS_BASE_ADDR = 0x200000
SPIFFS_SIZE = 0x010000
SPIFFS_LOG_PAGE_SIZE ?= 256
SPIFFS_LOG_BLOCK_SIZE ?= 8192

SDK_BASE   ?= /opt/esp-open-sdk/sdk
SDK_LIBDIR  = lib
SDK_INCDIR  = include include/json

ESPTOOL      ?= /opt/esp-open-sdk/esptool/esptool.py
ESPTOOL2     ?= /opt/esp-open-sdk/esptool2/esptool2

FW_SECTS      = .text .data .rodata
FW_USER_ARGS  = -quiet -bin -boot2 -iromchksum

ESPPORT     ?= /dev/ttyUSB0
ESPBAUD     ?= 57600
ESPBAUD2    ?= 19200

ifndef XTENSA_BINDIR
CC := xtensa-lx106-elf-gcc
LD := xtensa-lx106-elf-gcc
OBJDUMP := xtensa-lx106-elf-objdump
ELF_SIZE := xtensa-lx106-elf-size
else
CC := $(addprefix $(XTENSA_BINDIR)/,xtensa-lx106-elf-gcc)
LD := $(addprefix $(XTENSA_BINDIR)/,xtensa-lx106-elf-gcc)
OBJDUMP := $(addprefix $(XTENSA_BINDIR)/,xtensa-lx106-elf-objdump)
ELF_SIZE := $(addprefix $(XTENSA_BINDIR)/,xtensa-lx106-elf-size)
endif

# libmain must be modified for rBoot big flash support (just one symbol gets weakened)
LIBMAIN = main2
LIBMAIN_DST = $(addprefix $(BUILD_DIR)/,libmain2.a)
LIBMAIN_SRC = $(addprefix $(SDK_LIBDIR)/,libmain.a)

BUILD_DIR = build
FIRMW_DIR = firmware

SDK_LIBDIR := $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR := $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))


LIBS    = c gcc hal phy net80211 lwip wpa $(LIBMAIN) pp crypto json
CFLAGS  = -I. -w -Os -ggdb -std=c99 -g -Wpointer-arith -Wundef \
          -Wall -Werror -Wno-implicit -Wl,-EL -Wno-implicit-function-declaration \
          -fno-exceptions -fno-inline-functions -nostdlib -mlongcalls -mno-target-align \
          -Wextra -Wmissing-prototypes -Wstrict-prototypes \
          -flto=8 -flto-compression-level=0 -fuse-linker-plugin -ffat-lto-objects -flto-partition=max \
          -mtext-section-literals -ffunction-sections -fdata-sections \
          -fno-builtin-printf -fno-jump-tables -mno-serialize-volatile \
          -fno-guess-branch-probability -freorder-blocks-and-partition -fno-cse-follow-jumps \
          -D__ets__ -DICACHE_FLASH -DUSE_US_TIMER -DUSE_OPTIMIZE_PRINTF -DBOOT_BIG_FLASH
LDFLAGS = -nostdlib -u call_user_start -Wl,-gc-sections -Wl,--size-opt -Wl,-static -u Cache_Read_Enable_New 

SRC		:= $(wildcard *.c)
OBJ		:= $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC))
LIBS	:= $(addprefix -l,$(LIBS))

ifneq ($(WIFI_SSID), "")
	CFLAGS += -DWIFI_SSID=\"$(WIFI_SSID)\"
endif
ifneq ($(WIFI_PWD), "")
	CFLAGS += -DWIFI_PWD=\"$(WIFI_PWD)\"
endif

# SPI_SPEED = 40, 26, 20, 80
SPI_SPEED ?= 40
# SPI_MODE: qio, qout, dio, dout
SPI_MODE ?= DIO
# SPI_SIZE_MAP
# 0 : 512 KB (256 KB + 256 KB)
# 1 : 256 KB
# 2 : 1024 KB (512 KB + 512 KB)
# 3 : 2048 KB (512 KB + 512 KB)
# 4 : 4096 KB (512 KB + 512 KB)
# 5 : 2048 KB (1024 KB + 1024 KB)
# 6 : 4096 KB (1024 KB + 1024 KB)
SPI_SIZE_MAP ?= 6
ESP_ROM_MAX_SIZE ?=516096  # 0x7E000 - 504 KB max bin file   -   126 blocks

ifeq ($(SPI_SPEED), 26.7)
    freqdiv = 1
    flashimageoptions = -ff 26m
else
    ifeq ($(SPI_SPEED), 20)
        freqdiv = 2
        flashimageoptions = -ff 20m
    else
        ifeq ($(SPI_SPEED), 80)
            freqdiv = 15
            flashimageoptions = -ff 80m
        else
            freqdiv = 0
            flashimageoptions = -ff 40m
        endif
    endif
endif

ifeq ($(SPI_MODE), QOUT)
    mode = 1
    flashimageoptions += -fm qout
else
    ifeq ($(SPI_MODE), DIO)
        mode = 2
        flashimageoptions += -fm dio
    else
        ifeq ($(SPI_MODE), DOUT)
            mode = 3
            flashimageoptions += -fm dout
        else
            mode = 0
            flashimageoptions += -fm qio
        endif
    endif
endif

ifeq ($(SPI_SIZE_MAP), 1)
  size_map = 1
  flash = 256
  flashimageoptions += -fs 256KB
else
  ifeq ($(SPI_SIZE_MAP), 2)
    size_map = 2
    flash = 1024
    flashimageoptions += -fs 1MB
  else
    ifeq ($(SPI_SIZE_MAP), 3)
      size_map = 3
      flash = 2048
      flashimageoptions += -fs 2MB
    else
      ifeq ($(SPI_SIZE_MAP), 4)
		size_map = 4
		flash = 4096
		flashimageoptions += -fs 4MB
      else
        ifeq ($(SPI_SIZE_MAP), 5)
          size_map = 5
          flash = 2048
          flashimageoptions += -fs 2MB-c1
        else
          ifeq ($(SPI_SIZE_MAP), 6)
            size_map = 6
            flash = 4096
            flashimageoptions += -fs 4MB-c1
          else
            size_map = 0
            flash = 512
            flashimageoptions += -fs 512KB
          endif
        endif
      endif
    endif
  endif
endif

.SECONDARY:
.PHONY: all clean

MEM_USAGE = \
  'while (<>) { \
      $$r += $$1 if /^\.(?:data|rodata|bss)\s+(\d+)/;\
		  $$f += $$1 if /^\.(?:irom0\.text|text|data|rodata)\s+(\d+)/;\
	 }\
	 print "\# Memory usage\n";\
	 print sprintf("\#  %-6s %6d bytes\n" x 2 ."\n", "Ram:", $$r, "Flash:", $$f);'

C_FILES = $(wildcard *.c)
O_FILES = $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_FILES))

all: $(BUILD_DIR) $(FIRMW_DIR) $(LIBMAIN_DST) $(FIRMW_DIR)/rom0.bin

$(BUILD_DIR)/%.o: %.c %.h
	@echo "$(CC) $(SDK_INCDIR) $(CFLAGS) -o $@ -c $<"
	@$(CC) $(SDK_INCDIR) $(CFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.elf: $(O_FILES)
	@echo "\n### --- Build $@"
	@echo "LD $(notdir $@)"
	@echo "$(LD) -L$(SDK_LIBDIR) -T$(notdir $(basename $@)).ld $(LDFLAGS) -Wl,--start-group $(LIBS) $^ -Wl,--end-group -o $@"
	@$(LD) -L$(SDK_LIBDIR) -T$(notdir $(basename $@)).ld $(LDFLAGS) -Wl,--start-group $(LIBS) $^ -Wl,--end-group -o $@
	@echo "------------------------------------------------------------------------------"
	@echo "Section info:"
	@$(OBJDUMP) -h -j .data -j .rodata -j .bss -j .text -j .irom0.text $@
	@echo "------------------------------------------------------------------------------"
	@echo "Size info:"
	@$(ELF_SIZE) -A $@ |grep -v " 0$$" |grep .
	@$(ELF_SIZE) -A $@ | perl -e $(MEM_USAGE)
	@echo "------------------------------------------------------------------------------"

$(FIRMW_DIR)/%.bin: $(BUILD_DIR)/%.elf
	@echo "### --- Build $@"
	@echo "FW $(notdir $@)"
	@echo "$(ESPTOOL2) $(FW_USER_ARGS) $^ $@ $(FW_SECTS)"
	@$(ESPTOOL2) $(FW_USER_ARGS) $^ $@ $(FW_SECTS)
	@echo "------------------------------------------------------------------------------\n"
	@echo " >>  $(notdir $@) uses $$(stat -c '%s' $@) bytes of $$(( $(ESP_ROM_MAX_SIZE) )) available"
	@$(ESPTOOL) image_info $@
	@echo "------------------------------------------------------------------------------\n\n"

$(LIBMAIN_DST): $(LIBMAIN_SRC)
	@echo "OC $(notdir $@)"
	@$(OBJCOPY) -W Cache_Read_Enable_New $^ $@

$(BUILD_DIR):
	@mkdir -p $@

$(FIRMW_DIR):
	@mkdir -p $@

flash-rboot:
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD2) write_flash $(flashimageoptions) 0x00000 rboot.bin

flash-sampleproject:
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD2) write_flash $(flashimageoptions) 0x02000 $(FIRMW_DIR)/rom0.bin

flash-all: clean all erase_flash flash-rboot flash-sampleproject flash-init

flash-all2: all flash-rboot flash-sampleproject flash-init

flash-init:
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD2) write_flash $(flashimageoptions) \
		0x1fc000 $(SDK_BASE)/bin/esp_init_data_default_v08.bin

flash-only: erase_flash flash-rboot flash-sampleproject flash-init

erase-flash:
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) write_flash $(flashimageoptions) 0x00000 empty_target.bin

erase_flash:
	$(ESPTOOL) -p $(ESPPORT) erase_flash

flash_id:
	$(ESPTOOL) -p $(ESPPORT) flash_id

erase-param:
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD2) write_flash $(flashimageoptions) 0x3C000 blank4.bin
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD2) write_flash $(flashimageoptions) 0x3FE000 blank.bin

dump-flash:
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) read_flash 0x4000000 65536 dump_flash.bin

read-flash:
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) read_flash 0 4194304 read_flash.bin

param-flash:
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) read_flash 0x0x3F0000 4096 param_flash.bin

rebuild: clean all

monitor:
	picocom -b115200 $(ESPPORT)

clean:
	@echo "RM $(BUILD_DIR) $(FIRMW_DIR)"
	@rm -rf $(BUILD_DIR)
	@rm -rf $(FIRMW_DIR)

