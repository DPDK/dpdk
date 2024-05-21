# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Stephen Hemminger <stephen@networkplumber.org>

enable_tap_rss = false

libbpf = dependency('libbpf', required: false, method: 'pkg-config')
if not libbpf.found()
    message('net/tap: no RSS support missing libbpf')
    subdir_done()
endif

# Debian install this in /usr/sbin which is not in $PATH
bpftool = find_program('bpftool', '/usr/sbin/bpftool', required: false, version: '>= 5.6.0')
if not bpftool.found()
    message('net/tap: no RSS support missing bpftool')
    subdir_done()
endif

clang_supports_bpf = false
clang = find_program('clang', required: false)
if clang.found()
    clang_supports_bpf = run_command(clang, '-target', 'bpf', '--print-supported-cpus',
                                     check: false).returncode() == 0
endif

if not clang_supports_bpf
    message('net/tap: no RSS support missing clang BPF')
    subdir_done()
endif

enable_tap_rss = true

libbpf_include_dir = libbpf.get_variable(pkgconfig : 'includedir')

# The include files <linux/bpf.h> and others include <asm/types.h>
# but <asm/types.h> is not defined for multi-lib environment target.
# Workaround by using include directoriy from the host build environment.
machine_name = run_command('uname', '-m').stdout().strip()
march_include_dir = '/usr/include/' + machine_name + '-linux-gnu'

clang_flags = [
        '-O2',
        '-Wall',
        '-Wextra',
        '-target',
        'bpf',
        '-g',
        '-c',
]

bpf_o_cmd = [
        clang,
        clang_flags,
        '-idirafter',
        libbpf_include_dir,
        '-idirafter',
        march_include_dir,
        '@INPUT@',
        '-o',
        '@OUTPUT@',
]

skel_h_cmd = [
        bpftool,
        'gen',
        'skeleton',
        '@INPUT@',
]

tap_rss_o = custom_target(
        'tap_rss.bpf.o',
        input: 'tap_rss.c',
        output: 'tap_rss.o',
        command: bpf_o_cmd)

tap_rss_skel_h = custom_target(
        'tap_rss.skel.h',
        input: tap_rss_o,
        output: 'tap_rss.skel.h',
        command: skel_h_cmd,
        capture: true)
