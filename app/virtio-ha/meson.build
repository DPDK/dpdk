# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, NVIDIA CORPORATION & AFFILIATES.

sources = files('main.c')
deps += ['eal', 'common_virtio_ha']

conf = configuration_data()
conf.set('prefix', get_option('prefix'))

configure_file(input : 'vfe-vhostd-ha.service.in',
               output : 'vfe-vhostd-ha.service',
               configuration: conf,
               install_dir : '/usr/lib/systemd/system/')

# if meson >= 0.61.0, can use install_symlink instead
meson.add_install_script('sh', '-c',
                         'cd ${MESON_INSTALL_DESTDIR_PREFIX}/bin; ln -sf dpdk-virtio-ha vfe-vhostd-ha')
