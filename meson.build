#   BSD LICENSE
#
#   Copyright(c) 2017 Intel Corporation.
#   All rights reserved.
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
#     * Neither the name of Intel Corporation nor the names of its
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

project('DPDK', 'C',
	version: '18.02.0',
	license: 'BSD',
	default_options: ['buildtype=release'],
	meson_version: '>= 0.41'
)

# set up some global vars for compiler, platform, configuration, etc.
cc = meson.get_compiler('c')
dpdk_conf = configuration_data()
dpdk_libraries = []
dpdk_drivers = []
dpdk_extra_ldflags = []

# for static libs, treat the drivers as regular libraries, otherwise
# for shared libs, put them in a driver folder
if get_option('default_library') == 'static'
	driver_install_path = get_option('libdir')
	eal_pmd_path = ''
else
	driver_install_path = join_paths(get_option('datadir'), 'dpdk/drivers')
	eal_pmd_path = join_paths(get_option('prefix'), driver_install_path)
endif

# configure the build, and make sure configs here and in config folder are
# able to be included in any file. We also store a global array of include dirs
# for passing to pmdinfogen scripts
global_inc = include_directories('.', 'config')
subdir('config')

# TODO build libs and drivers

# TODO build binaries and installable tools

# write the build config
build_cfg = 'rte_build_config.h'
configure_file(output: build_cfg,
		configuration: dpdk_conf,
		install_dir: get_option('includedir'))

# for static builds, include the drivers as libs, and also any
# other dependent libs that DPDK needs to link against
if get_option('default_library') == 'static'
	dpdk_drivers = ['-Wl,--whole-archive'] + dpdk_drivers + ['-Wl,--no-whole-archive']
	dpdk_libraries = dpdk_drivers + dpdk_libraries + dpdk_extra_ldflags
endif

pkg = import('pkgconfig')
pkg.generate(name: meson.project_name(),
	version: meson.project_version(),
	libraries: dpdk_libraries,
	description: 'The Data Plane Development Kit (DPDK)',
	extra_cflags: ['-include', 'rte_config.h', '-march=@0@'.format(machine)]
)
