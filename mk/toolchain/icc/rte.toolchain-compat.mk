#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
endif
