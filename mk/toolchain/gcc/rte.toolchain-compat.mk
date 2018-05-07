# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# CPUID-related options
#
# This was added to support compiler versions which might not support all the
# flags we need
#

#find out GCC version

GCC_MAJOR = $(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR = $(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)
GCC_PATCHLEVEL = $(shell echo __GNUC_PATCHLEVEL__ | $(CC) -E -x c - | tail -n 1)
GCC_VERSION = $(GCC_MAJOR)$(GCC_MINOR)

# if GCC is older than 4.x
ifeq ($(shell test $(GCC_VERSION) -lt 40 && echo 1), 1)
	MACHINE_CFLAGS =
$(warning You are using GCC < 4.x. This is neither supported, nor tested.)


else
# GCC graceful degradation
# GCC 4.2.x - added support for generic target
# GCC 4.3.x - added support for core2, ssse3, sse4.1, sse4.2
# GCC 4.4.x - added support for avx, aes, pclmul
# GCC 4.5.x - added support for atom
# GCC 4.6.x - added support for corei7, corei7-avx
# GCC 4.7.x - added support for fsgsbase, rdrnd, f16c, core-avx-i, core-avx2
# GCC 4.9.x - added support for armv8-a+crc
#
	ifeq ($(shell test $(GCC_VERSION) -le 49 && echo 1), 1)
		MACHINE_CFLAGS := $(patsubst -march=armv8-a+crc,-march=armv8-a+crc -D__ARM_FEATURE_CRC32=1,$(MACHINE_CFLAGS))
		MACHINE_CFLAGS := $(patsubst -march=armv8-a+crc+crypto,-march=armv8-a+crc+crypto -D__ARM_FEATURE_CRC32=1,$(MACHINE_CFLAGS))
	endif
	ifeq ($(shell test $(GCC_VERSION) -le 47 && echo 1), 1)
		MACHINE_CFLAGS := $(patsubst -march=core-avx-i,-march=corei7-avx,$(MACHINE_CFLAGS))
		MACHINE_CFLAGS := $(patsubst -march=core-avx2,-march=core-avx2,$(MACHINE_CFLAGS))
	endif
	ifeq ($(shell test $(GCC_VERSION) -lt 46 && echo 1), 1)
		MACHINE_CFLAGS := $(patsubst -march=corei7-avx,-march=core2 -maes -mpclmul -mavx,$(MACHINE_CFLAGS))
		MACHINE_CFLAGS := $(patsubst -march=corei7,-march=core2 -maes -mpclmul,$(MACHINE_CFLAGS))
	endif
	ifeq ($(shell test $(GCC_VERSION) -lt 45 && echo 1), 1)
		MACHINE_CFLAGS := $(patsubst -march=atom,-march=core2 -mssse3,$(MACHINE_CFLAGS))
	endif
	ifeq ($(shell test $(GCC_VERSION) -lt 44 && echo 1), 1)
		MACHINE_CFLAGS := $(filter-out -mavx -mpclmul -maes,$(MACHINE_CFLAGS))
		ifneq ($(findstring SSE4_2, $(CPUFLAGS)),)
			MACHINE_CFLAGS += -msse4.2
		endif
		ifneq ($(findstring SSE4_1, $(CPUFLAGS)),)
			MACHINE_CFLAGS += -msse4.1
		endif
	endif
	ifeq ($(shell test $(GCC_VERSION) -lt 43 && echo 1), 1)
		MACHINE_CFLAGS := $(filter-out -msse% -mssse%,$(MACHINE_CFLAGS))
		MACHINE_CFLAGS := $(patsubst -march=core2,-march=generic,$(MACHINE_CFLAGS))
		MACHINE_CFLAGS += -msse3
	endif
	ifeq ($(shell test $(GCC_VERSION) -lt 42 && echo 1), 1)
		MACHINE_CFLAGS := $(filter-out -march% -mtune% -msse%,$(MACHINE_CFLAGS))
	endif

	# Disable thunderx PMD for gcc < 4.7
	ifeq ($(shell test $(GCC_VERSION) -lt 47 && echo 1), 1)
		CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD=d
	endif

	# Disable OPDL PMD for gcc < 4.7
	ifeq ($(shell test $(GCC_VERSION) -lt 47 && echo 1), 1)
		CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV=d
	endif

	# Disable octeontx event PMD for gcc < 4.8.6
	ifeq ($(shell test $(GCC_VERSION)$(GCC_PATCHLEVEL) -lt 486 && echo 1), 1)
		CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF=d
		CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL=d
		CONFIG_RTE_LIBRTE_OCTEONTX_PMD=d
	endif

endif
