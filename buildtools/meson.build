# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

pkgconf = find_program('pkg-config', 'pkgconf', required: false)
pmdinfo = find_program('gen-pmdinfo-cfile.sh')
check_symbols = find_program('check-symbols.sh')
ldflags_ibverbs_static = find_program('options-ibverbs-static.sh')
binutils_avx512_check = find_program('binutils-avx512-check.sh')

python3 = import('python').find_installation(required: false)
if python3.found()
	py3 = [python3]
else
	py3 = ['meson', 'runpython']
endif
echo = py3 + ['-c', 'import sys; print(*sys.argv[1:])']
list_dir_globs = py3 + files('list-dir-globs.py')
map_to_win_cmd = py3 + files('map_to_win.py')
sphinx_wrapper = py3 + files('call-sphinx-build.py')
