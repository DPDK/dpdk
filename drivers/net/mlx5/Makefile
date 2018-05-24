#   SPDX-License-Identifier: BSD-3-Clause
#   Copyright 2015 6WIND S.A.
#   Copyright 2015 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# Library name.
LIB = librte_pmd_mlx5.a
LIB_GLUE = $(LIB_GLUE_BASE).$(LIB_GLUE_VERSION)
LIB_GLUE_BASE = librte_pmd_mlx5_glue.so
LIB_GLUE_VERSION = 18.05.0

# Sources.
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5.c
ifneq ($(CONFIG_RTE_LIBRTE_MLX5_DLOPEN_DEPS),y)
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_glue.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rxq.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_txq.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rxtx.c
ifneq ($(filter y,$(CONFIG_RTE_ARCH_X86_64) \
		  $(CONFIG_RTE_ARCH_ARM64)),)
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rxtx_vec.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_trigger.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rxmode.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_vlan.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_stats.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rss.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_mr.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_socket.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_nl.c

ifeq ($(CONFIG_RTE_LIBRTE_MLX5_DLOPEN_DEPS),y)
INSTALL-$(CONFIG_RTE_LIBRTE_MLX5_PMD)-lib += $(LIB_GLUE)
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
CFLAGS += -Wno-strict-prototypes
ifeq ($(CONFIG_RTE_LIBRTE_MLX5_DLOPEN_DEPS),y)
CFLAGS += -DMLX5_GLUE='"$(LIB_GLUE)"'
CFLAGS += -DMLX5_GLUE_VERSION='"$(LIB_GLUE_VERSION)"'
CFLAGS_mlx5_glue.o += -fPIC
LDLIBS += -ldl
else
LDLIBS += -libverbs -lmlx5
endif
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

# A few warnings cannot be avoided in external headers.
CFLAGS += -Wno-error=cast-qual

EXPORT_MAP := rte_pmd_mlx5_version.map
LIBABIVER := 1

# memseg walk is not part of stable API
CFLAGS += -DALLOW_EXPERIMENTAL_API

# DEBUG which is usually provided on the command-line may enable
# CONFIG_RTE_LIBRTE_MLX5_DEBUG.
ifeq ($(DEBUG),1)
CONFIG_RTE_LIBRTE_MLX5_DEBUG := y
endif

# User-defined CFLAGS.
ifeq ($(CONFIG_RTE_LIBRTE_MLX5_DEBUG),y)
CFLAGS += -pedantic -UNDEBUG -DPEDANTIC
else
CFLAGS += -DNDEBUG -UPEDANTIC
endif

include $(RTE_SDK)/mk/rte.lib.mk

# Generate and clean-up mlx5_autoconf.h.

export CC CFLAGS CPPFLAGS EXTRA_CFLAGS EXTRA_CPPFLAGS
export AUTO_CONFIG_CFLAGS = -Wno-error

ifndef V
AUTOCONF_OUTPUT := >/dev/null
endif

mlx5_autoconf.h.new: FORCE

mlx5_autoconf.h.new: $(RTE_SDK)/buildtools/auto-config-h.sh
	$Q $(RM) -f -- '$@'
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT \
		infiniband/mlx5dv.h \
		enum MLX5DV_CONTEXT_MASK_STRIDING_RQ \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_TUNNEL_SUPPORT \
		infiniband/mlx5dv.h \
		enum MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_MPLS_SUPPORT \
		infiniband/verbs.h \
		enum IBV_FLOW_SPEC_MPLS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_WQ_FLAG_RX_END_PADDING \
		infiniband/verbs.h \
		enum IBV_WQ_FLAG_RX_END_PADDING \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_MLX5_MOD_SWP \
		infiniband/mlx5dv.h \
		type 'struct mlx5dv_sw_parsing_caps' \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_MLX5_MOD_MPW \
		infiniband/mlx5dv.h \
		enum MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_MLX5_MOD_CQE_128B_COMP \
		infiniband/mlx5dv.h \
		enum MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_ETHTOOL_LINK_MODE_25G \
		/usr/include/linux/ethtool.h \
		enum ETHTOOL_LINK_MODE_25000baseCR_Full_BIT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_ETHTOOL_LINK_MODE_50G \
		/usr/include/linux/ethtool.h \
		enum ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_ETHTOOL_LINK_MODE_100G \
		/usr/include/linux/ethtool.h \
		enum ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT \
		infiniband/verbs.h \
		enum IBV_FLOW_SPEC_ACTION_COUNT \
		$(AUTOCONF_OUTPUT)

# Create mlx5_autoconf.h or update it in case it differs from the new one.

mlx5_autoconf.h: mlx5_autoconf.h.new
	$Q [ -f '$@' ] && \
		cmp '$<' '$@' $(AUTOCONF_OUTPUT) || \
		mv '$<' '$@'

$(SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD):.c=.o): mlx5_autoconf.h

# Generate dependency plug-in for rdma-core when the PMD must not be linked
# directly, so that applications do not inherit this dependency.

ifeq ($(CONFIG_RTE_LIBRTE_MLX5_DLOPEN_DEPS),y)

$(LIB): $(LIB_GLUE)

ifeq ($(LINK_USING_CC),1)
GLUE_LDFLAGS := $(call linkerprefix,$(LDFLAGS))
else
GLUE_LDFLAGS := $(LDFLAGS)
endif
$(LIB_GLUE): mlx5_glue.o
	$Q $(LD) $(GLUE_LDFLAGS) $(EXTRA_LDFLAGS) \
		-Wl,-h,$(LIB_GLUE) \
		-s -shared -o $@ $< -libverbs -lmlx5

mlx5_glue.o: mlx5_autoconf.h

endif

clean_mlx5: FORCE
	$Q rm -f -- mlx5_autoconf.h mlx5_autoconf.h.new
	$Q rm -f -- mlx5_glue.o $(LIB_GLUE_BASE)*

clean: clean_mlx5
