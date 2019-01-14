# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# this makefile is called from the generic rte.vars.mk and is
# used to set the RTE_CPUFLAG_* environment variables giving details
# of what instruction sets the target cpu supports.

AUTO_CPUFLAGS := $(shell $(CC) $(MACHINE_CFLAGS) $(WERROR_FLAGS) $(EXTRA_CFLAGS) -dM -E - < /dev/null)

# adding flags to CPUFLAGS

ifneq ($(filter $(AUTO_CPUFLAGS),__SSE__),)
CPUFLAGS += SSE
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__SSE2__),)
CPUFLAGS += SSE2
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__SSE3__),)
CPUFLAGS += SSE3
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__SSSE3__),)
CPUFLAGS += SSSE3
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__SSE4_1__),)
CPUFLAGS += SSE4_1
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__SSE4_2__),)
CPUFLAGS += SSE4_2
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__AES__),)
CPUFLAGS += AES
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__PCLMUL__),)
CPUFLAGS += PCLMULQDQ
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__AVX__),)
ifeq ($(CONFIG_RTE_ENABLE_AVX),y)
CPUFLAGS += AVX
endif
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__RDRND__),)
CPUFLAGS += RDRAND
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__FSGSBASE__),)
CPUFLAGS += FSGSBASE
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__F16C__),)
CPUFLAGS += F16C
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__AVX2__),)
ifeq ($(CONFIG_RTE_ENABLE_AVX),y)
CPUFLAGS += AVX2
endif
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__AVX512F__),)
ifeq ($(CONFIG_RTE_ENABLE_AVX512),y)
CPUFLAGS += AVX512F
else
# disable AVX512F support for GCC & binutils 2.30 as a workaround for Bug 97
ifeq ($(FORCE_DISABLE_AVX512),y)
MACHINE_CFLAGS += -mno-avx512f
endif
endif
endif

# IBM Power CPU flags
ifneq ($(filter $(AUTO_CPUFLAGS),__PPC64__),)
CPUFLAGS += PPC64
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__PPC32__),)
CPUFLAGS += PPC32
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__vector),)
CPUFLAGS += ALTIVEC
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__builtin_vsx_xvnmaddadp),)
CPUFLAGS += VSX
endif

# ARM flags
ifneq ($(filter __ARM_NEON __aarch64__,$(AUTO_CPUFLAGS)),)
CPUFLAGS += NEON
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__ARM_FEATURE_CRC32),)
CPUFLAGS += CRC32
endif

ifneq ($(filter $(AUTO_CPUFLAGS),__ARM_FEATURE_CRYPTO),)
CPUFLAGS += AES
CPUFLAGS += PMULL
CPUFLAGS += SHA1
CPUFLAGS += SHA2
endif

MACHINE_CFLAGS += $(addprefix -DRTE_MACHINE_CPUFLAG_,$(CPUFLAGS))

# To strip whitespace
comma:= ,
empty:=
space:= $(empty) $(empty)
CPUFLAGSTMP1 := $(addprefix RTE_CPUFLAG_,$(CPUFLAGS))
CPUFLAGSTMP2 := $(subst $(space),$(comma),$(CPUFLAGSTMP1))
CPUFLAGS_LIST := -DRTE_COMPILE_TIME_CPUFLAGS=$(CPUFLAGSTMP2)
