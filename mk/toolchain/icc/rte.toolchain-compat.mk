# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# CPUID-related options
#
# This was added to support compiler versions which might not support all the
# flags we need
#

# find out ICC version

ICC_MAJOR_VERSION = $(shell icc -dumpversion | cut -f1 -d.)

ifeq ($(shell test $(ICC_MAJOR_VERSION) -lt 12 && echo 1), 1)
	MACHINE_CFLAGS = -xSSE4.2
$(warning You are not using ICC 12.x or higher. This is neither supported, nor tested.)

else
# proceed to adjust compiler flags

	ICC_MINOR_VERSION = $(shell icc -dumpversion | cut -f2 -d.)

# replace GCC flags with ICC flags
	# if icc version >= 12
	ifeq ($(shell test $(ICC_MAJOR_VERSION) -ge 12 && echo 1), 1)
		# Atom
		MACHINE_CFLAGS := $(patsubst -march=atom,-xSSSE3_ATOM -march=atom,$(MACHINE_CFLAGS))
		# nehalem/westmere
		MACHINE_CFLAGS := $(patsubst -march=corei7,-xSSE4.2 -march=corei7,$(MACHINE_CFLAGS))
		# sandy bridge
		MACHINE_CFLAGS := $(patsubst -march=corei7-avx,-xAVX,$(MACHINE_CFLAGS))
		# ivy bridge
		MACHINE_CFLAGS := $(patsubst -march=core-avx-i,-xCORE-AVX-I,$(MACHINE_CFLAGS))
		# hsw
		MACHINE_CFLAGS := $(patsubst -march=core-avx2,-xCORE-AVX2,$(MACHINE_CFLAGS))
		# remove westmere flags
		MACHINE_CFLAGS := $(filter-out -mpclmul -maes,$(MACHINE_CFLAGS))
	endif
	# if icc version == 12.0
	ifeq ($(shell test $(ICC_MAJOR_VERSION) -eq 12 && test $(ICC_MINOR_VERSION) -eq 0 && echo 1), 1)
		# Atom
		MACHINE_CFLAGS := $(patsubst -xSSSE3_ATOM,-xSSE3_ATOM,$(MACHINE_CFLAGS))
		# remove march options
		MACHINE_CFLAGS := $(patsubst -march=%,-xSSE3,$(MACHINE_CFLAGS))
	endif

	# Disable thunderx PMD for icc <= 16.0
	ifeq ($(shell test $(ICC_MAJOR_VERSION) -le 16 && echo 1), 1)
		CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD=d
	endif

        # Disable event/opdl  PMD for icc <= 16.0
	ifeq ($(shell test $(ICC_MAJOR_VERSION) -le 16 && echo 1), 1)
		CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV=d
	endif

endif
