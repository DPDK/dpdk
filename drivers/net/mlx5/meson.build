# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 6WIND S.A.
# Copyright 2018 Mellanox Technologies, Ltd

pmd_dlopen = get_option('enable_driver_mlx_glue')
LIB_GLUE_BASE = 'librte_pmd_mlx5_glue.so'
LIB_GLUE_VERSION = '18.05.0'
LIB_GLUE = LIB_GLUE_BASE + '.' + LIB_GLUE_VERSION
if pmd_dlopen
	dpdk_conf.set('RTE_LIBRTE_MLX5_DLOPEN_DEPS', 1)
	cflags += [
		'-DMLX5_GLUE="@0@"'.format(LIB_GLUE),
		'-DMLX5_GLUE_VERSION="@0@"'.format(LIB_GLUE_VERSION),
	]
endif
libs = [
	cc.find_library('mnl', required:false),
	cc.find_library('mlx5', required:false),
	cc.find_library('ibverbs', required:false),
]
build = true
foreach lib:libs
	if not lib.found()
		build = false
	endif
endforeach
if build
	allow_experimental_apis = true
	ext_deps += libs
	sources = files(
		'mlx5.c',
		'mlx5_ethdev.c',
		'mlx5_flow.c',
		'mlx5_mac.c',
		'mlx5_mr.c',
		'mlx5_nl.c',
		'mlx5_nl_flow.c',
		'mlx5_rss.c',
		'mlx5_rxmode.c',
		'mlx5_rxq.c',
		'mlx5_rxtx.c',
		'mlx5_socket.c',
		'mlx5_stats.c',
		'mlx5_trigger.c',
		'mlx5_txq.c',
		'mlx5_vlan.c',
	)
	if dpdk_conf.has('RTE_ARCH_X86_64') or dpdk_conf.has('RTE_ARCH_ARM64')
		sources += files('mlx5_rxtx_vec.c')
	endif
	if not pmd_dlopen
		sources += files('mlx5_glue.c')
	endif
	cflags_options = [
		'-Wextra',
		'-std=c11',
		'-Wno-strict-prototypes',
		'-D_BSD_SOURCE',
		'-D_DEFAULT_SOURCE',
		'-D_XOPEN_SOURCE=600'
	]
	foreach option:cflags_options
		if cc.has_argument(option)
			cflags += option
		endif
	endforeach
	if get_option('buildtype').contains('debug')
		cflags += [ '-pedantic', '-UNDEBUG', '-DPEDANTIC' ]
	else
		cflags += [ '-DNDEBUG', '-UPEDANTIC' ]
	endif
	# To maintain the compatibility with the make build system
	# mlx5_autoconf.h file is still generated.
	# input array for meson member search:
	# [ "MACRO to define if found", "header for the search",
	#   "symbol to search", "struct member to search" ]
	has_member_args = [
		[ 'HAVE_IBV_MLX5_MOD_SWP', 'infiniband/mlx5dv.h',
		'struct mlx5dv_sw_parsing_caps', 'sw_parsing_offloads' ],
		[ 'HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT', 'infiniband/verbs.h',
		'struct ibv_counter_set_init_attr', 'counter_set_id' ],
	]
	# input array for meson symbol search:
	# [ "MACRO to define if found", "header for the search",
	#   "symbol to search" ]
	has_sym_args = [
		[ 'HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT', 'infiniband/mlx5dv.h',
		'MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX' ],
		[ 'HAVE_IBV_DEVICE_TUNNEL_SUPPORT', 'infiniband/mlx5dv.h',
		'MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS' ],
		[ 'HAVE_IBV_MLX5_MOD_MPW', 'infiniband/mlx5dv.h',
		'MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED' ],
		[ 'HAVE_IBV_MLX5_MOD_CQE_128B_COMP', 'infiniband/mlx5dv.h',
		'MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP' ],
		[ 'HAVE_IBV_DEVICE_MPLS_SUPPORT', 'infiniband/verbs.h',
		'IBV_FLOW_SPEC_MPLS' ],
		[ 'HAVE_IBV_WQ_FLAG_RX_END_PADDING', 'infiniband/verbs.h',
		'IBV_WQ_FLAG_RX_END_PADDING' ],
		[ 'HAVE_SUPPORTED_40000baseKR4_Full', 'linux/ethtool.h',
		'SUPPORTED_40000baseKR4_Full' ],
		[ 'HAVE_SUPPORTED_40000baseCR4_Full', 'linux/ethtool.h',
		'SUPPORTED_40000baseCR4_Full' ],
		[ 'HAVE_SUPPORTED_40000baseSR4_Full', 'linux/ethtool.h',
		'SUPPORTED_40000baseSR4_Full' ],
		[ 'HAVE_SUPPORTED_40000baseLR4_Full', 'linux/ethtool.h',
		'SUPPORTED_40000baseLR4_Full' ],
		[ 'HAVE_SUPPORTED_56000baseKR4_Full', 'linux/ethtool.h',
		'SUPPORTED_56000baseKR4_Full' ],
		[ 'HAVE_SUPPORTED_56000baseCR4_Full', 'linux/ethtool.h',
		'SUPPORTED_56000baseCR4_Full' ],
		[ 'HAVE_SUPPORTED_56000baseSR4_Full', 'linux/ethtool.h',
		'SUPPORTED_56000baseSR4_Full' ],
		[ 'HAVE_SUPPORTED_56000baseLR4_Full', 'linux/ethtool.h',
		'SUPPORTED_56000baseLR4_Full' ],
		[ 'HAVE_ETHTOOL_LINK_MODE_25G', 'linux/ethtool.h',
		'ETHTOOL_LINK_MODE_25000baseCR_Full_BIT' ],
		[ 'HAVE_ETHTOOL_LINK_MODE_50G', 'linux/ethtool.h',
		'ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT' ],
		[ 'HAVE_ETHTOOL_LINK_MODE_100G', 'linux/ethtool.h',
		'ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT' ],
		[ 'HAVE_IFLA_PHYS_SWITCH_ID', 'linux/if_link.h',
		'IFLA_PHYS_SWITCH_ID' ],
		[ 'HAVE_IFLA_PHYS_PORT_NAME', 'linux/if_link.h',
		'IFLA_PHYS_PORT_NAME' ],
		[ 'HAVE_TCA_FLOWER_ACT', 'linux/pkt_cls.h',
		'TCA_FLOWER_ACT' ],
		[ 'HAVE_TCA_FLOWER_FLAGS', 'linux/pkt_cls.h',
		'TCA_FLOWER_FLAGS' ],
		[ 'HAVE_TCA_FLOWER_KEY_ETH_TYPE', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_ETH_TYPE' ],
		[ 'HAVE_TCA_FLOWER_KEY_ETH_DST', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_ETH_DST' ],
		[ 'HAVE_TCA_FLOWER_KEY_ETH_DST_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_ETH_DST_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_ETH_SRC', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_ETH_SRC' ],
		[ 'HAVE_TCA_FLOWER_KEY_ETH_SRC_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_ETH_SRC_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_IP_PROTO', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IP_PROTO' ],
		[ 'HAVE_TCA_FLOWER_KEY_IPV4_SRC', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IPV4_SRC' ],
		[ 'HAVE_TCA_FLOWER_KEY_IPV4_SRC_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IPV4_SRC_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_IPV4_DST', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IPV4_DST' ],
		[ 'HAVE_TCA_FLOWER_KEY_IPV4_DST_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IPV4_DST_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_IPV6_SRC', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IPV6_SRC' ],
		[ 'HAVE_TCA_FLOWER_KEY_IPV6_SRC_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IPV6_SRC_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_IPV6_DST', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IPV6_DST' ],
		[ 'HAVE_TCA_FLOWER_KEY_IPV6_DST_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_IPV6_DST_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_TCP_SRC', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_TCP_SRC' ],
		[ 'HAVE_TCA_FLOWER_KEY_TCP_SRC_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_TCP_SRC_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_TCP_DST', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_TCP_DST' ],
		[ 'HAVE_TCA_FLOWER_KEY_TCP_DST_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_TCP_DST_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_UDP_SRC', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_UDP_SRC' ],
		[ 'HAVE_TCA_FLOWER_KEY_UDP_SRC_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_UDP_SRC_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_UDP_DST', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_UDP_DST' ],
		[ 'HAVE_TCA_FLOWER_KEY_UDP_DST_MASK', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_UDP_DST_MASK' ],
		[ 'HAVE_TCA_FLOWER_KEY_VLAN_ID', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_VLAN_ID' ],
		[ 'HAVE_TCA_FLOWER_KEY_VLAN_PRIO', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_VLAN_PRIO' ],
		[ 'HAVE_TCA_FLOWER_KEY_VLAN_ETH_TYPE', 'linux/pkt_cls.h',
		'TCA_FLOWER_KEY_VLAN_ETH_TYPE' ],
		[ 'HAVE_TC_ACT_VLAN', 'linux/tc_act/tc_vlan.h',
		'TCA_VLAN_PUSH_VLAN_PRIORITY' ],
		[ 'HAVE_RDMA_NL_NLDEV', 'rdma/rdma_netlink.h',
		'RDMA_NL_NLDEV' ],
		[ 'HAVE_RDMA_NLDEV_CMD_GET', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_CMD_GET' ],
		[ 'HAVE_RDMA_NLDEV_CMD_PORT_GET', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_CMD_PORT_GET' ],
		[ 'HAVE_RDMA_NLDEV_ATTR_DEV_INDEX', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_ATTR_DEV_INDEX' ],
		[ 'HAVE_RDMA_NLDEV_ATTR_DEV_NAME', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_ATTR_DEV_NAME' ],
		[ 'HAVE_RDMA_NLDEV_ATTR_PORT_INDEX', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_ATTR_PORT_INDEX' ],
		[ 'HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_ATTR_NDEV_INDEX' ],
	]
	config = configuration_data()
	foreach arg:has_sym_args
		config.set(arg[0], cc.has_header_symbol(arg[1], arg[2]))
	endforeach
	foreach arg:has_member_args
		file_prefix = '#include<' + arg[1] + '>'
		config.set(arg[0], cc.has_member(arg[2], arg[3],
			prefix : file_prefix))
	endforeach
	configure_file(output : 'mlx5_autoconf.h', configuration : config)
endif
# Build Glue Library
if pmd_dlopen and build
	dlopen_name = 'mlx5_glue'
	dlopen_lib_name = driver_name_fmt.format(dlopen_name)
	dlopen_so_version = LIB_GLUE_VERSION
	dlopen_sources = files('mlx5_glue.c')
	dlopen_install_dir = [ eal_pmd_path + '-glue' ]
	shared_lib = shared_library(
		dlopen_lib_name,
		dlopen_sources,
		include_directories: global_inc,
		c_args: cflags,
		dependencies: libs,
		link_args: [
		'-Wl,-export-dynamic',
		'-Wl,-h,@0@'.format(LIB_GLUE),
		],
		soversion: dlopen_so_version,
		install: true,
		install_dir: dlopen_install_dir,
	)
endif
