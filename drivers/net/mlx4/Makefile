#   BSD LICENSE
#
#   Copyright 2012 6WIND S.A.
#   Copyright 2012 Mellanox
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
#     * Neither the name of 6WIND S.A. nor the names of its
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

# Library name.
LIB = librte_pmd_mlx4.a
LIB_GLUE = $(LIB_GLUE_BASE).$(LIB_GLUE_VERSION)
LIB_GLUE_BASE = librte_pmd_mlx4_glue.so
LIB_GLUE_VERSION = 18.02.0

# Sources.
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_flow.c
ifneq ($(CONFIG_RTE_LIBRTE_MLX4_DLOPEN_DEPS),y)
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_glue.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_intr.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_mr.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_rxq.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_txq.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4_utils.c

ifeq ($(CONFIG_RTE_LIBRTE_MLX4_DLOPEN_DEPS),y)
INSTALL-$(CONFIG_RTE_LIBRTE_MLX4_PMD)-lib += $(LIB_GLUE)
endif

# Basic CFLAGS.
CFLAGS += -O3
CFLAGS += -std=c11 -Wall -Wextra
CFLAGS += -g
CFLAGS += -I.
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -D_XOPEN_SOURCE=600
CFLAGS += $(WERROR_FLAGS)
ifeq ($(CONFIG_RTE_LIBRTE_MLX4_DLOPEN_DEPS),y)
CFLAGS += -DMLX4_GLUE='"$(LIB_GLUE)"'
CFLAGS += -DMLX4_GLUE_VERSION='"$(LIB_GLUE_VERSION)"'
CFLAGS_mlx4_glue.o += -fPIC
LDLIBS += -ldl
else
LDLIBS += -libverbs -lmlx4
endif
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

# A few warnings cannot be avoided in external headers.
CFLAGS += -Wno-error=cast-qual

EXPORT_MAP := rte_pmd_mlx4_version.map
LIBABIVER := 1

# DEBUG which is usually provided on the command-line may enable
# CONFIG_RTE_LIBRTE_MLX4_DEBUG.
ifeq ($(DEBUG),1)
CONFIG_RTE_LIBRTE_MLX4_DEBUG := y
endif

# User-defined CFLAGS.
ifeq ($(CONFIG_RTE_LIBRTE_MLX4_DEBUG),y)
CFLAGS += -pedantic -UNDEBUG -DPEDANTIC
else
CFLAGS += -DNDEBUG -UPEDANTIC
endif

ifdef CONFIG_RTE_LIBRTE_MLX4_TX_MP_CACHE
CFLAGS += -DMLX4_PMD_TX_MP_CACHE=$(CONFIG_RTE_LIBRTE_MLX4_TX_MP_CACHE)
endif

include $(RTE_SDK)/mk/rte.lib.mk

# Generate and clean-up mlx4_autoconf.h.

export CC CFLAGS CPPFLAGS EXTRA_CFLAGS EXTRA_CPPFLAGS
export AUTO_CONFIG_CFLAGS = -Wno-error

ifndef V
AUTOCONF_OUTPUT := >/dev/null
endif

mlx4_autoconf.h.new: FORCE

mlx4_autoconf.h.new: $(RTE_SDK)/buildtools/auto-config-h.sh
	$Q $(RM) -f -- '$@'
	$Q : > '$@'

# Create mlx4_autoconf.h or update it in case it differs from the new one.

mlx4_autoconf.h: mlx4_autoconf.h.new
	$Q [ -f '$@' ] && \
		cmp '$<' '$@' $(AUTOCONF_OUTPUT) || \
		mv '$<' '$@'

$(SRCS-$(CONFIG_RTE_LIBRTE_MLX4_PMD):.c=.o): mlx4_autoconf.h

# Generate dependency plug-in for rdma-core when the PMD must not be linked
# directly, so that applications do not inherit this dependency.

ifeq ($(CONFIG_RTE_LIBRTE_MLX4_DLOPEN_DEPS),y)

$(LIB): $(LIB_GLUE)

$(LIB_GLUE): mlx4_glue.o
	$Q $(LD) $(LDFLAGS) $(EXTRA_LDFLAGS) \
		-Wl,-h,$(LIB_GLUE) \
		-s -shared -o $@ $< -libverbs -lmlx4

mlx4_glue.o: mlx4_autoconf.h

endif

clean_mlx4: FORCE
	$Q rm -f -- mlx4_autoconf.h mlx4_autoconf.h.new
	$Q rm -f -- mlx4_glue.o $(LIB_GLUE_BASE)*

clean: clean_mlx4
