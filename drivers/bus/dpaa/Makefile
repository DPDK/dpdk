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
RTE_BUS_DPAA=$(RTE_SDK)/drivers/bus/dpaa

#
# library name
#
LIB = librte_bus_dpaa.a

CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -Wno-pointer-arith
CFLAGS += -Wno-cast-qual
CFLAGS += -D _GNU_SOURCE
CFLAGS += -I$(RTE_BUS_DPAA)/
CFLAGS += -I$(RTE_BUS_DPAA)/include
CFLAGS += -I$(RTE_BUS_DPAA)/base/qbman
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linuxapp/eal
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include

# versioning export map
EXPORT_MAP := rte_bus_dpaa_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_DPAA_BUS) += \
	dpaa_bus.c

SRCS-$(CONFIG_RTE_LIBRTE_DPAA_BUS) += \
	base/fman/fman.c \
	base/fman/fman_hw.c \
	base/fman/of.c \
	base/fman/netcfg_layer.c \
	base/qbman/process.c \
	base/qbman/bman.c \
	base/qbman/bman_driver.c \
	base/qbman/qman.c \
	base/qbman/qman_driver.c \
	base/qbman/dpaa_alloc.c \
	base/qbman/dpaa_sys.c

# Link Pthread
LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev

include $(RTE_SDK)/mk/rte.lib.mk
