# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# toolchain:
#
#   - define CC, LD, AR, AS, ... (overridden by cmdline value)
#   - define TOOLCHAIN_CFLAGS variable (overridden by cmdline value)
#   - define TOOLCHAIN_LDFLAGS variable (overridden by cmdline value)
#   - define TOOLCHAIN_ASFLAGS variable (overridden by cmdline value)
#

CC        = $(CROSS)gcc
KERNELCC  = $(CROSS)gcc
CPP       = $(CROSS)cpp
# for now, we don't use as but nasm.
# AS      = $(CROSS)as
AS        = nasm
AR        = $(CROSS)ar
LD        = $(CROSS)ld
OBJCOPY   = $(CROSS)objcopy
OBJDUMP   = $(CROSS)objdump
STRIP     = $(CROSS)strip
READELF   = $(CROSS)readelf
GCOV      = $(CROSS)gcov

ifeq ("$(origin CC)", "command line")
HOSTCC    = $(CC)
else
HOSTCC    = gcc
endif
HOSTAS    = as

TOOLCHAIN_ASFLAGS =
TOOLCHAIN_CFLAGS =
TOOLCHAIN_LDFLAGS =

ifeq ($(CONFIG_RTE_LIBRTE_GCOV),y)
TOOLCHAIN_CFLAGS += --coverage
TOOLCHAIN_LDFLAGS += --coverage
ifeq (,$(findstring -O0,$(EXTRA_CFLAGS)))
  $(warning "EXTRA_CFLAGS doesn't contains -O0, coverage will be inaccurate with optimizations enabled")
endif
endif

WERROR_FLAGS := -W -Wall -Wstrict-prototypes -Wmissing-prototypes
WERROR_FLAGS += -Wmissing-declarations -Wold-style-definition -Wpointer-arith
WERROR_FLAGS += -Wcast-align -Wnested-externs -Wcast-qual
WERROR_FLAGS += -Wformat-nonliteral -Wformat-security
WERROR_FLAGS += -Wundef -Wwrite-strings -Wdeprecated

ifeq ($(RTE_DEVEL_BUILD),y)
WERROR_FLAGS += -Werror
endif

# There are many issues reported for strict alignment architectures
# which are not necessarily fatal. Report as warnings.
ifeq ($(CONFIG_RTE_ARCH_STRICT_ALIGN),y)
WERROR_FLAGS += -Wno-error=cast-align
endif

# process cpu flags
include $(RTE_SDK)/mk/toolchain/$(RTE_TOOLCHAIN)/rte.toolchain-compat.mk

# workaround GCC bug with warning "missing initializer" for "= {0}"
ifeq ($(shell test $(GCC_VERSION) -lt 47 && echo 1), 1)
WERROR_FLAGS += -Wno-missing-field-initializers
endif
# workaround GCC bug with warning "may be used uninitialized"
ifeq ($(shell test $(GCC_VERSION) -lt 47 && echo 1), 1)
WERROR_FLAGS += -Wno-uninitialized
endif

ifeq ($(shell test $(GCC_VERSION) -gt 70 && echo 1), 1)
# Tell GCC only to error for switch fallthroughs without a suitable comment
WERROR_FLAGS += -Wimplicit-fallthrough=2
# Ignore errors for snprintf truncation
WERROR_FLAGS += -Wno-format-truncation
endif

export CC AS AR LD OBJCOPY OBJDUMP STRIP READELF
export TOOLCHAIN_CFLAGS TOOLCHAIN_LDFLAGS TOOLCHAIN_ASFLAGS
