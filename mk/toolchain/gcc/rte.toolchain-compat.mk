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

#find out GCC version

GCC_VERSION = $(subst .,,$(shell $(CC) -dumpversion | cut -f1-2 -d.))

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
endif
