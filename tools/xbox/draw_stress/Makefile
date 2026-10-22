# SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds draw_stress.nro, the open-source draw-heavy workload for per-draw CPU profiling (#207).
#
# Requires devkitPro with switch-dev, switch-mesa and switch-glad (all in devkitpro/devkita64). From this directory:
#     C:\devkitPro\msys2\usr\bin\make.exe
# The result, draw_stress.nro, is loaded like boot.nro (see docs/NOTES.md).

# GCC needs a writable directory for its temporaries. msys2's make hands the recipe an
# environment with TMP/TEMP/TMPDIR all unset, so GCC on Windows falls back to GetTempPath(),
# which resolves to C:\Windows - not writable, and the compile dies with a confusing
# "Cannot create temporary file". Point it somewhere it can actually write.
ifeq ($(strip $(TMPDIR)),)
export TMPDIR := $(CURDIR)
endif

DEVKITPRO ?= C:/devkitPro
# libnx's switch.specs expands $DEVKITPRO itself, so it has to reach GCC as a real
# environment variable, not just a make variable.
export DEVKITPRO
LIBNX     := $(DEVKITPRO)/libnx
CC        := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-gcc
# Mesa (the GL driver linked in) is C++, so the final link needs g++ for libstdc++.
CXX       := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-g++
ELF2NRO   := $(DEVKITPRO)/tools/bin/elf2nro

# Matches the devkitPro switch template: Cortex-A57 (the Switch's CPU), soft thread pointer, PIE.
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS  := -g -Wall -O2 -ffunction-sections $(ARCH) -I$(LIBNX)/include -D__SWITCH__
LDFLAGS := -specs=$(LIBNX)/switch.specs -g $(ARCH)
PORTLIBS := $(DEVKITPRO)/portlibs/switch
CFLAGS  += -I$(PORTLIBS)/include
LIBS    := -L$(PORTLIBS)/lib -L$(LIBNX)/lib -lglad -lEGL -lglapi -ldrm_nouveau -lnx -lm

TARGET := draw_stress

all: $(TARGET).nro

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: main.o
	$(CXX) $(LDFLAGS) $< $(LIBS) -o $@

$(TARGET).nro: $(TARGET).elf
	$(ELF2NRO) $< $@

clean:
	rm -f main.o $(TARGET).elf $(TARGET).nro

.PHONY: all clean
