#   SPDX-License-Identifier: BSD-3-Clause
#   Copyright 2015 6WIND S.A.
#   Copyright 2015 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# Library name.
LIB = librte_pmd_mlx5.a
LIB_GLUE = $(LIB_GLUE_BASE).$(LIB_GLUE_VERSION)
LIB_GLUE_BASE = librte_pmd_mlx5_glue.so
LIB_GLUE_VERSION = 19.05.0

# Sources.
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5.c
ifneq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)
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
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_flow_dv.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_flow_tcf.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_flow_verbs.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_mp.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_nl.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_devx_cmds.c

ifeq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)
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
CFLAGS += $(shell command -v pkg-config > /dev/null 2>&1 && pkg-config --cflags libmnl)
ifeq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)
CFLAGS += -DMLX5_GLUE='"$(LIB_GLUE)"'
CFLAGS += -DMLX5_GLUE_VERSION='"$(LIB_GLUE_VERSION)"'
CFLAGS_mlx5_glue.o += -fPIC
LDLIBS += -ldl
else ifeq ($(CONFIG_RTE_IBVERBS_LINK_STATIC),y)
LDLIBS += $(shell $(RTE_SDK)/buildtools/options-ibverbs-static.sh)
else
LDLIBS += -libverbs -lmlx5
endif
LDLIBS += $(shell command -v pkg-config > /dev/null 2>&1 && pkg-config --libs libmnl || echo "-lmnl")
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
		enum MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX \
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
		HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING \
		infiniband/verbs.h \
		enum IBV_WQ_FLAGS_PCI_WRITE_END_PADDING \
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
		HAVE_IBV_MLX5_MOD_CQE_128B_PAD \
		infiniband/mlx5dv.h \
		enum MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_FLOW_DV_SUPPORT \
		infiniband/mlx5dv.h \
		func mlx5dv_create_flow_action_packet_reformat \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DR \
		infiniband/mlx5dv.h \
		enum MLX5DV_DR_DOMAIN_TYPE_NIC_RX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DR_ESWITCH \
		infiniband/mlx5dv.h \
		enum MLX5DV_DR_DOMAIN_TYPE_FDB \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVX_OBJ \
		infiniband/mlx5dv.h \
		func mlx5dv_devx_obj_create \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_FLOW_DEVX_COUNTERS \
		infiniband/mlx5dv.h \
		enum MLX5DV_FLOW_ACTION_COUNTERS_DEVX \
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
		HAVE_IBV_DEVICE_COUNTERS_SET_V42 \
		infiniband/verbs.h \
		type 'struct ibv_counter_set_init_attr' \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_COUNTERS_SET_V45 \
		infiniband/verbs.h \
		type 'struct ibv_counters_init_attr' \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NL_NLDEV \
		rdma/rdma_netlink.h \
		enum RDMA_NL_NLDEV \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_CMD_GET \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_CMD_GET \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_CMD_PORT_GET \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_CMD_PORT_GET \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_ATTR_DEV_INDEX \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_ATTR_DEV_INDEX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_ATTR_DEV_NAME \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_ATTR_DEV_NAME \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_ATTR_PORT_INDEX \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_ATTR_PORT_INDEX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_ATTR_NDEV_INDEX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_NUM_VF \
		linux/if_link.h \
		enum IFLA_NUM_VF \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_EXT_MASK \
		linux/if_link.h \
		enum IFLA_EXT_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_PHYS_SWITCH_ID \
		linux/if_link.h \
		enum IFLA_PHYS_SWITCH_ID \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_PHYS_PORT_NAME \
		linux/if_link.h \
		enum IFLA_PHYS_PORT_NAME \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_VXLAN_COLLECT_METADATA \
		linux/if_link.h \
		enum IFLA_VXLAN_COLLECT_METADATA \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_CHAIN \
		linux/rtnetlink.h \
		enum TCA_CHAIN \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_ACT \
		linux/pkt_cls.h \
		enum TCA_FLOWER_ACT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_FLAGS \
		linux/pkt_cls.h \
		enum TCA_FLOWER_FLAGS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ETH_TYPE \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ETH_TYPE \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ETH_DST \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ETH_DST \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ETH_DST_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ETH_DST_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ETH_SRC \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ETH_SRC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ETH_SRC_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ETH_SRC_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IP_PROTO \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IP_PROTO \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IPV4_SRC \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IPV4_SRC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IPV4_SRC_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IPV4_SRC_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IPV4_DST \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IPV4_DST \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IPV4_DST_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IPV4_DST_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IPV6_SRC \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IPV6_SRC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IPV6_SRC_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IPV6_SRC_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IPV6_DST \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IPV6_DST \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IPV6_DST_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IPV6_DST_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_TCP_SRC \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_TCP_SRC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_TCP_SRC_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_TCP_SRC_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_TCP_DST \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_TCP_DST \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_TCP_DST_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_TCP_DST_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_UDP_SRC \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_UDP_SRC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_UDP_SRC_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_UDP_SRC_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_UDP_DST \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_UDP_DST \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_UDP_DST_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_UDP_DST_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_VLAN_ID \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_VLAN_ID \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_VLAN_PRIO \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_VLAN_PRIO \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_VLAN_ETH_TYPE \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_VLAN_ETH_TYPE \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_TCP_FLAGS \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_TCP_FLAGS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_TCP_FLAGS_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_TCP_FLAGS_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IP_TOS \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IP_TOS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IP_TOS_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IP_TOS_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IP_TTL \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IP_TTL \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_IP_TTL_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_IP_TTL_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TC_ACT_GOTO_CHAIN \
		linux/pkt_cls.h \
		define TC_ACT_GOTO_CHAIN \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TC_ACT_VLAN \
		linux/tc_act/tc_vlan.h \
		enum TCA_VLAN_PUSH_VLAN_PRIORITY \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_KEY_ID \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_KEY_ID \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IPV4_SRC \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IPV4_SRC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IPV4_DST \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IPV4_DST \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IPV4_DST_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IPV4_DST_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IPV6_SRC \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IPV6_SRC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IPV6_DST \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IPV6_DST \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IPV6_DST_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IPV6_DST_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_UDP_SRC_PORT \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_UDP_SRC_PORT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_UDP_SRC_PORT_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_UDP_SRC_PORT_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_UDP_DST_PORT \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_UDP_DST_PORT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_UDP_DST_PORT_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_UDP_DST_PORT_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IP_TOS \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IP_TOS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IP_TOS_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IP_TOS_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IP_TTL \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IP_TTL \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_FLOWER_KEY_ENC_IP_TTL_MASK \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_ENC_IP_TTL_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TC_ACT_TUNNEL_KEY \
		linux/tc_act/tc_tunnel_key.h \
		define TCA_ACT_TUNNEL_KEY \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_TUNNEL_KEY_ENC_DST_PORT \
		linux/tc_act/tc_tunnel_key.h \
		enum TCA_TUNNEL_KEY_ENC_DST_PORT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_TUNNEL_KEY_ENC_TOS \
		linux/tc_act/tc_tunnel_key.h \
		enum TCA_TUNNEL_KEY_ENC_TOS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_TUNNEL_KEY_ENC_TTL \
		linux/tc_act/tc_tunnel_key.h \
		enum TCA_TUNNEL_KEY_ENC_TTL \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TCA_TUNNEL_KEY_NO_CSUM \
		linux/tc_act/tc_tunnel_key.h \
		enum TCA_TUNNEL_KEY_NO_CSUM \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TC_ACT_PEDIT \
		linux/tc_act/tc_pedit.h \
		enum TCA_PEDIT_KEY_EX_HDR_TYPE_UDP \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_40000baseKR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_40000baseKR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_40000baseCR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_40000baseCR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_40000baseSR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_40000baseSR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_40000baseLR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_40000baseLR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_56000baseKR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_56000baseKR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_56000baseCR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_56000baseCR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_56000baseSR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_56000baseSR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_56000baseLR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_56000baseLR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_STATIC_ASSERT \
		/usr/include/assert.h \
		define static_assert \
		$(AUTOCONF_OUTPUT)

# Create mlx5_autoconf.h or update it in case it differs from the new one.

mlx5_autoconf.h: mlx5_autoconf.h.new
	$Q [ -f '$@' ] && \
		cmp '$<' '$@' $(AUTOCONF_OUTPUT) || \
		mv '$<' '$@'

$(SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD):.c=.o): mlx5_autoconf.h

# Generate dependency plug-in for rdma-core when the PMD must not be linked
# directly, so that applications do not inherit this dependency.

ifeq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)

$(LIB): $(LIB_GLUE)

ifeq ($(LINK_USING_CC),1)
GLUE_LDFLAGS := $(call linkerprefix,$(LDFLAGS))
else
GLUE_LDFLAGS := $(LDFLAGS)
endif
$(LIB_GLUE): mlx5_glue.o
	$Q $(LD) $(GLUE_LDFLAGS) $(EXTRA_LDFLAGS) \
		-Wl,-h,$(LIB_GLUE) \
		-shared -o $@ $< -libverbs -lmlx5

mlx5_glue.o: mlx5_autoconf.h

endif

clean_mlx5: FORCE
	$Q rm -f -- mlx5_autoconf.h mlx5_autoconf.h.new
	$Q rm -f -- mlx5_glue.o $(LIB_GLUE_BASE)*

clean: clean_mlx5
