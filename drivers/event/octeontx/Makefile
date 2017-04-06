#   BSD LICENSE
#
#   Copyright(c) 2017 Cavium Networks. All rights reserved.
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
#     * Neither the name of Cavium Networks nor the names of its
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

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_octeontx_ssovf.a

CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_octeontx_ssovf_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += ssovf_worker.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += ssovf_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += ssovf_probe.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += ssovf_mbox.c

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_ssovf_worker.o += -fno-prefetch-loop-arrays

ifeq ($(shell test $(GCC_VERSION) -ge 46 && echo 1), 1)
CFLAGS_ssovf_worker.o += -Ofast
else
CFLAGS_ssovf_worker.o += -O3 -ffast-math
endif

else
CFLAGS_ssovf_worker.o += -Ofast
endif

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF)-include := rte_pmd_octeontx_ssovf.h

include $(RTE_SDK)/mk/rte.lib.mk
