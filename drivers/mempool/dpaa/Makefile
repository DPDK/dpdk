#   BSD LICENSE
#
#   Copyright 2016 NXP.
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
#     * Neither the name of NXP nor the names of its
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

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_mempool_dpaa.a

CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -D _GNU_SOURCE
CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa
CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa/include/
CFLAGS += -I$(RTE_SDK)/drivers/mempool/dpaa
CFLAGS += -I$(RTE_SDK)/lib/librte_mempool

# versioning export map
EXPORT_MAP := rte_mempool_dpaa_version.map

# Lbrary version
LIBABIVER := 1

# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_DPAA_MEMPOOL) += dpaa_mempool.c

LDLIBS += -lrte_bus_dpaa
LDLIBS += -lrte_eal -lrte_mempool -lrte_ring

include $(RTE_SDK)/mk/rte.lib.mk
