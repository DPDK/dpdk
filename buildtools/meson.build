# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

subdir('pmdinfogen')

pkgconf = find_program('pkg-config', 'pkgconf', required: false)
pmdinfo = find_program('gen-pmdinfo-cfile.sh')
list_dir_globs = find_program('list-dir-globs.py')
check_symbols = find_program('check-symbols.sh')
ldflags_ibverbs_static = find_program('options-ibverbs-static.sh')

# set up map-to-def script using python, either built-in or external
python3 = import('python').find_installation(required: false)
if python3.found()
	py3 = [python3]
else
	py3 = ['meson', 'runpython']
endif
map_to_def_cmd = py3 + files('map_to_def.py')
sphinx_wrapper = py3 + files('call-sphinx-build.py')

# stable ABI always starts with "DPDK_"
is_stable_cmd = [find_program('grep', 'findstr'), '^DPDK_']
