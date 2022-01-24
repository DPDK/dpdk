# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

# For SUSE build check function arguments of ndo_tx_timeout API
# Ref: https://jira.devtools.intel.com/browse/DPDK-29263
kmod_cflags = ''
file_path = kernel_source_dir + '/include/linux/netdevice.h'
run_cmd = run_command('grep', 'ndo_tx_timeout', file_path, check: false)

if run_cmd.stdout().contains('txqueue') == true
   kmod_cflags = '-DHAVE_ARG_TX_QUEUE'
endif


kni_mkfile = custom_target('rte_kni_makefile',
        output: 'Makefile',
        command: ['touch', '@OUTPUT@'])

kni_sources = files(
        'kni_misc.c',
        'kni_net.c',
        'Kbuild',
)

custom_target('rte_kni',
        input: kni_sources,
        output: 'rte_kni.ko',
        command: ['make', '-j4', '-C', kernel_build_dir,
            'M=' + meson.current_build_dir(),
            'src=' + meson.current_source_dir(),
            ' '.join(['MODULE_CFLAGS=', kmod_cflags,'-include '])
            + dpdk_source_root + '/config/rte_config.h' +
            ' -I' + dpdk_source_root + '/lib/eal/include' +
            ' -I' + dpdk_source_root + '/lib/kni' +
            ' -I' + dpdk_build_root +
            ' -I' + meson.current_source_dir(),
            'modules'] + cross_args,
        depends: kni_mkfile,
        install: install,
        install_dir: kernel_install_dir,
        build_by_default: get_option('enable_kmods'))
