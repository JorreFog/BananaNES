# BananaNES build: banananes_libretro.so for the RG DS / RG DS Plus (Cortex-A55) or the PC (host, tests/profiling).
#   make -C banana -j4                       # RG DS Plus (BOARD=rgds for the RG DS), build/rgds-plus/banananes_libretro.so
#   make -C banana HOST=1 -j4                # PC build, build/host/
#   make -C banana BTAG=name OPT="-O3 ..."   # variants (A/B): build/<board>-name/
# Heavy builds go through ~/overnight-2026-11-01/buildslot.sh, -j4 at most.
CORE_DIR := $(abspath $(CURDIR)/../src)
ROOT := $(abspath $(CURDIR)/..)
GIT_VERSION ?= banana-$(shell git -C $(ROOT) rev-parse --short HEAD 2>/dev/null)
HAVE_NTSC ?= 1
HAVE_HDPACK ?= 1
WANT_32BPP ?= 1
# PGO: banana/pgo holds the .gcda files of a training run on the handheld (banana/tools/pgo-train.sh); they are copied next to the
# objects and used with the default flags (NO_PGO=1 turns it off; a source change makes the profile of the changed functions unused)
PGO_DIR ?= $(ROOT)/banana/pgo
ifeq ($(NO_PGO)$(HOST),)
ifneq ($(wildcard $(PGO_DIR)/ppu.gcda),)
PGOFLAGS := -fprofile-use -fprofile-correction -Wno-missing-profile -Wno-coverage-mismatch
endif
endif
OPT ?= -O3 -flto=4 -fno-semantic-interposition $(PGOFLAGS)
BANANA_DEFS ?=
ifdef BUILDROOT
  # Buildroot package: CC/AR come from the package's command line, the toolchain wrapper already knows the sysroot
  BTAG_BASE := target
  ARCH_FLAGS ?= -mcpu=cortex-a55
  SYSFLAGS :=
else ifdef HOST
  BTAG_BASE := host
  CC := gcc
  AR := ar
  ARCH_FLAGS ?=
  SYSFLAGS :=
else
  include $(HOME)/banana-emu/common/a55.mk
  BTAG_BASE := $(BOARD)
  ARCH_FLAGS ?= -mcpu=cortex-a55
  SYSFLAGS := --sysroot=$(A55_SYSROOT)
endif
ifneq ($(PGO_FIXED)$(PGOFLAGS),)
# GCC's profile data identifies static functions by the object file's name: PGO builds (generate, train, use) always compile
# from the repo root with the same relative paths (build/pgo/x.o from src/x.c), wherever the tree is
BDIR := $(ROOT)/build/pgo
else
BDIR := $(ROOT)/build/$(BTAG_BASE)$(if $(BTAG),-$(BTAG))
endif
include $(ROOT)/Makefile.common
ifeq ($(HAVE_HDPACK),1)
  COREDEFINES += -DHAVE_HDPACK
endif
CFLAGS := $(SYSFLAGS) $(ARCH_FLAGS) $(OPT) -DNDEBUG -fPIC -pipe -fno-plt $(COREDEFINES) $(BANANA_DEFS) -DGIT_VERSION=\"$(GIT_VERSION)\" \
          -Wno-write-strings -Wno-unused-result $(INCFLAGS)
LDFLAGS := $(SYSFLAGS) -fPIC -shared -Wl,--version-script=$(CORE_DIR)/drivers/libretro/link.T -Wl,-no-undefined -Wl,-O1 -Wl,--as-needed $(ARCH_FLAGS) $(OPT) $(LTO_LD)
OBJS := $(patsubst $(CORE_DIR)/%.c,$(BDIR)/%.o,$(SOURCES_C))
TARGET := $(BDIR)/banananes_libretro.so
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lm
ifneq ($(X6502_OPT),)
$(BDIR)/x6502.o: CFLAGS += $(X6502_OPT)
endif
$(BDIR)/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(dir $@)
	@if [ -n "$(PGOFLAGS)" ] && [ -f $(PGO_DIR)/$*.gcda ]; then cp -f $(PGO_DIR)/$*.gcda $(BDIR)/$*.gcda; fi
ifneq ($(PGO_FIXED)$(PGOFLAGS),)
	cd $(ROOT) && $(CC) -c -o build/pgo/$*.o src/$*.c $(CFLAGS)
else
	$(CC) -c -o $@ $< $(CFLAGS)
endif
clean:
	rm -rf $(BDIR)
