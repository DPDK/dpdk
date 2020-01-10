# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

subdir('pmdinfogen')

pmdinfo = find_program('gen-pmdinfo-cfile.sh')

check_experimental_syms = find_program('check-experimental-syms.sh')

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
is_experimental_cmd = [find_program('grep', 'findstr'), '^DPDK_']
