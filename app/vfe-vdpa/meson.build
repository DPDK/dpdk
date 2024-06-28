# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, NVIDIA CORPORATION & AFFILIATES.

if has_libev == 0
	build = false
	reason = 'missing dependency, "libev"'
	subdir_done()
endif

# Depends on libuuid
if has_libuuid == 0
	build = false
	reason = 'missing dependency, "libuuid"'
	subdir_done()
endif

sources = files('cJSON.c', 'jsonrpc-c.c', 'jsonrpc-client.c', 'main.c', 'vdpa_rpc.c')
headers = files('cJSON.h', 'jsonrpc-c.h', 'jsonrpc-client.h', 'vdpa_rpc.h')
deps += ['vhost', 'ethdev', 'cmdline', 'vdpa_virtio', 'common_virtio', 'common_virtio_mi','common_virtio_ha']

install_data([
            'vhostmgmt',
            'check_pf_reset.sh'
        ],
        install_dir: 'bin')

conf = configuration_data()
conf.set('prefix', get_option('prefix'))

configure_file(input : 'vfe-vhostd.service.in',
               output : 'vfe-vhostd.service',
               configuration: conf,
               install_dir : '/usr/lib/systemd/system/')

# if meson >= 0.61.0, can use install_symlink instead
meson.add_install_script('sh', '-c',
                         'cd ${MESON_INSTALL_DESTDIR_PREFIX}/bin; ln -sf vhostmgmt vfe-vhost-cli')
meson.add_install_script('sh', '-c',
                         'cd ${MESON_INSTALL_DESTDIR_PREFIX}/bin; ln -sf dpdk-vfe-vdpa vfe-vhostd')

