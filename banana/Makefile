# BananaNES build: banananes_libretro.so for the RG DS / RG DS Plus (Cortex-A55) or the PC (host, tests/profiling).
#   make -C banana -j4                       # RG DS Plus (BOARD=rgds for the RG DS), build/rgds-plus/banananes_libretro.so
#   make -C banana HOST=1 -j4                # PC build, build/host/
#   make -C banana BTAG=name OPT="-O3 ..."   # variants (A/B): build/<board>-name/
# Heavy builds go through ~/overnight-2026-11-01/buildslot.sh, -j4 at most.
CORE_DIR := $(abspath $(CURDIR)/../src)
ROOT := $(abspath $(CURDIR)/..)
HAVE_NTSC ?= 1
HAVE_HDPACK ?= 1
WANT_32BPP ?= 0
OPT ?= -O3 -flto=4 -fno-semantic-interposition
BANANA_DEFS ?=
ifdef HOST
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
BDIR := $(ROOT)/build/$(BTAG_BASE)$(if $(BTAG),-$(BTAG))
include $(ROOT)/Makefile.common
ifeq ($(HAVE_HDPACK),1)
  COREDEFINES += -DHAVE_HDPACK
endif
CFLAGS := $(SYSFLAGS) $(ARCH_FLAGS) $(OPT) -DNDEBUG -fPIC -pipe -fno-plt $(COREDEFINES) $(BANANA_DEFS) -DGIT_VERSION=\"banana\" \
          -Wno-write-strings -Wno-unused-result $(INCFLAGS)
LDFLAGS := $(SYSFLAGS) -fPIC -shared -Wl,--version-script=$(CORE_DIR)/drivers/libretro/link.T -Wl,-no-undefined -Wl,-O1 -Wl,--as-needed $(ARCH_FLAGS) $(OPT) $(LTO_LD)
OBJS := $(patsubst $(CORE_DIR)/%.c,$(BDIR)/%.o,$(SOURCES_C))
TARGET := $(BDIR)/banananes_libretro.so
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lm
$(BDIR)/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(CFLAGS)
clean:
	rm -rf $(BDIR)
