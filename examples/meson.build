# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

link_whole_libs = []
if get_option('default_library') == 'static'
    link_whole_libs = dpdk_static_libraries + dpdk_drivers
endif

# list of all example apps. Keep 1-3 per line, in alphabetical order.
all_examples = [
        'bbdev_app',
        'bond',
        'cmdline',
        'distributor',
        'dma',
        'ethtool',
        'eventdev_pipeline',
        'fips_validation',
        'flow_filtering',
        'helloworld',
        'ip_fragmentation',
        'ip_pipeline',
        'ip_reassembly',
        'ipsec-secgw',
        'ipv4_multicast',
        'l2fwd',
        'l2fwd-cat',
        'l2fwd-crypto',
        'l2fwd-event',
        'l2fwd-jobstats',
        'l2fwd-keepalive',
        'l3fwd',
        'l3fwd-graph',
        'l3fwd-power',
        'link_status_interrupt',
        'multi_process/client_server_mp/mp_client',
        'multi_process/client_server_mp/mp_server',
        'multi_process/hotplug_mp',
        'multi_process/simple_mp',
        'multi_process/symmetric_mp',
        'ntb',
        'packet_ordering',
        'pipeline',
        'ptpclient',
        'qos_meter',
        'qos_sched',
        'rxtx_callbacks',
        'server_node_efd/efd_node',
        'server_node_efd/efd_server',
        'service_cores',
        'skeleton',
        'timer',
        'vdpa',
        'vhost',
        'vhost_blk',
        'vhost_crypto',
        'vm_power_manager',
        'vm_power_manager/guest_cli',
        'vmdq',
        'vmdq_dcb',
]

# on install, skip copying all meson.build files
ex_file_excludes = ['meson.build']
foreach ex:all_examples
    ex_file_excludes += [ex + '/meson.build']
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
default_ldflags = dpdk_extra_ldflags
if get_option('default_library') == 'static' and not is_windows
    default_ldflags += ['-Wl,--export-dynamic']
endif

foreach example: examples
    name = example.split('/')[-1]
    build = true
    sources = []
    allow_experimental_apis = false
    cflags = default_cflags
    ldflags = default_ldflags

    ext_deps = []
    includes = [include_directories(example, 'common')]
    deps = ['eal', 'mempool', 'net', 'mbuf', 'ethdev', 'cmdline']
    subdir(example)

    if build
        dep_objs = ext_deps
        foreach d:deps
            var_name = get_option('default_library') + '_rte_' + d
            if not is_variable(var_name)
                build = false
                message('Missing dependency "@0@" for example "@1@"'.format(d, name))
                break
            endif
            dep_objs += [get_variable(var_name)]
        endforeach
    endif

    if not build
        if not allow_skips
            error('Cannot build requested example "' + name + '"')
        endif
        message('Skipping example "' + name + '"')
        continue
    endif

    if allow_experimental_apis
        cflags += '-DALLOW_EXPERIMENTAL_API'
    endif
    executable('dpdk-' + name, sources,
            include_directories: includes,
            link_whole: link_whole_libs,
            link_args: ldflags,
            c_args: cflags,
            dependencies: dep_objs)
endforeach
