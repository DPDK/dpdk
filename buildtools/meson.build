# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

pkgconf = find_program('pkg-config', 'pkgconf', required: false)
check_symbols = find_program('check-symbols.sh')
ldflags_ibverbs_static = find_program('options-ibverbs-static.sh')

python3_required_modules = []
if host_machine.system() != 'windows'
    python3_required_modules = ['elftools']
endif
python3 = import('python').find_installation('python3', required: false, modules: python3_required_modules)
if python3.found()
    py3 = [python3]
else
    py3 = ['meson', 'runpython']
endif
echo = py3 + ['-c', 'import sys; print(*sys.argv[1:])']
gen_version_map = py3 + files('gen-version-map.py')
list_dir_globs = py3 + files('list-dir-globs.py')
sphinx_wrapper = py3 + files('call-sphinx-build.py')
get_cpu_count_cmd = py3 + files('get-cpu-count.py')
get_numa_count_cmd = py3 + files('get-numa-count.py')
get_min_meson_version_cmd = py3 + files('get-min-meson-version.py')
get_test_suites_cmd = py3 + files('get-test-suites.py')
header_gen_cmd = py3 + files('gen-header.py')
has_hugepages_cmd = py3 + files('has-hugepages.py')
cmdline_gen_cmd = py3 + files('dpdk-cmdline-gen.py')
check_dts_requirements = py3 + files('check-dts-requirements.py')

# check that we can correctly report minimum Meson version
min_ver = run_command(get_min_meson_version_cmd, check: true, capture: true)
message('Minimum Meson required version is ' + min_ver.stdout())

# install any build tools that end-users might want also
install_data([
            'dpdk-cmdline-gen.py',
        ],
        install_dir: 'bin')

# select library and object file format
pmdinfo = py3 + files('gen-pmdinfo-cfile.py')
pmdinfogen = py3 + files('pmdinfogen.py')
if host_machine.system() == 'windows'
    if cc.get_id() == 'gcc'
        pmdinfo += 'ar'
    elif cc.get_id() == 'msvc'
        pmdinfo += 'lib'
    else
        pmdinfo += 'llvm-ar'
    endif
    pmdinfogen += 'coff'
elif host_machine.system() == 'freebsd'
    pmdinfo += 'llvm-ar'
    pmdinfogen += 'elf'
else
    pmdinfo += 'ar'
    pmdinfogen += 'elf'
endif
