# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

link_whole_libs = []
if get_option('default_library') == 'static'
	link_whole_libs = dpdk_static_libraries + dpdk_drivers
endif

execinfo = cc.find_library('execinfo', required: false)

# list of all example apps. Keep 1-3 per line, in alphabetical order.
all_examples = [
	'bbdev_app', 'bond',
	'cmdline',
	'distributor', 'ethtool',
	'eventdev_pipeline',
	'fips_validation', 'flow_classify',
	'flow_filtering', 'helloworld',
	'ioat',
	'ip_fragmentation', 'ip_pipeline',
	'ip_reassembly', 'ipsec-secgw',
	'ipv4_multicast', 'kni',
	'l2fwd', 'l2fwd-cat', 'l2fwd-event',
	'l2fwd-crypto', 'l2fwd-jobstats',
	'l2fwd-keepalive', 'l3fwd',
	'l3fwd-acl', 'l3fwd-power', 'l3fwd-graph',
	'link_status_interrupt',
	'multi_process/client_server_mp/mp_client',
	'multi_process/client_server_mp/mp_server',
	'multi_process/hotplug_mp',
	'multi_process/simple_mp',
	'multi_process/symmetric_mp',
	'ntb', 'packet_ordering',
	'performance-thread/l3fwd-thread',
	'performance-thread/pthread_shim',
	'ptpclient',
	'qos_meter', 'qos_sched',
	'rxtx_callbacks',
	'server_node_efd/node',
	'server_node_efd/server',
	'service_cores',
	'skeleton', 'tep_termination',
	'timer', 'vdpa',
	'vhost', 'vhost_crypto',
	'vhost_blk', 'vm_power_manager',
	'vm_power_manager/guest_cli',
	'vmdq', 'vmdq_dcb',
]
# install all example code on install - irrespective of whether the example in
# question is to be built as part of this build or not.
foreach ex:all_examples
	install_subdir(ex,
			install_dir: get_option('datadir') + '/dpdk/examples',
			exclude_files: 'meson.build')
endforeach

if get_option('examples') == ''
	subdir_done()
endif

if get_option('examples').to_lower() == 'all'
	examples = all_examples
	allow_skips = true # don't flag an error if we can't build an app
else
	examples = get_option('examples').split(',')
	allow_skips = false # error out if we can't build a requested app
endif
default_cflags = machine_args
if cc.has_argument('-Wno-format-truncation')
	default_cflags += '-Wno-format-truncation'
endif

foreach example: examples
	name = example.split('/')[-1]
	build = true
	sources = []
	allow_experimental_apis = false
	cflags = default_cflags

	ext_deps = [execinfo]
	includes = [include_directories(example)]
	deps = ['eal', 'mempool', 'net', 'mbuf', 'ethdev', 'cmdline']
	if is_windows
		deps = ['eal'] # only supported lib on Windows currently
	endif
	subdir(example)

	if build
		dep_objs = ext_deps
		foreach d:deps
			var_name = get_option('default_library') + '_rte_' + d
			if not is_variable(var_name)
				error('Missing dependency "@0@" for example "@1@"'.format(d, name))
			endif
			dep_objs += [get_variable(var_name)]
		endforeach
		if allow_experimental_apis
			cflags += '-DALLOW_EXPERIMENTAL_API'
		endif
		executable('dpdk-' + name, sources,
			include_directories: includes,
			link_whole: link_whole_libs,
			link_args: dpdk_extra_ldflags,
			c_args: cflags,
			dependencies: dep_objs)
	elif not allow_skips
		error('Cannot build requested example "' + name + '"')
	else
		message('Skipping example "' + name + '"')
	endif
endforeach
