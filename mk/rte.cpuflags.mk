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

# this makefile is called from the generic rte.vars.mk and is
# used to set the RTE_CPUFLAG_* environment variables giving details
# of what instruction sets the target cpu supports.

AUTO_CPUFLAGS := $(shell $(CC) $(MACHINE_CFLAGS) -dM -E - < /dev/null)

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
CPUFLAGS += AVX
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
CPUFLAGS += AVX2
endif

MACHINE_CFLAGS += $(addprefix -DRTE_MACHINE_CPUFLAG_,$(CPUFLAGS))

# To strip whitespace
comma:= ,
empty:=
space:= $(empty) $(empty)
CPUFLAGSTMP1 := $(addprefix RTE_CPUFLAG_,$(CPUFLAGS))
CPUFLAGSTMP2 := $(subst $(space),$(comma),$(CPUFLAGSTMP1))
MACHINE_CFLAGS += -DRTE_COMPILE_TIME_CPUFLAGS=$(CPUFLAGSTMP2)
