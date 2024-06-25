# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Stephen Hemminger <stephen@networkplumber.org>

# Loading BPF requires libbpf
# and the bpf_map__XXX API's were introduced in 0.8.0
libbpf = dependency('libbpf', version: '>= 1.0',
                    required: false, method: 'pkg-config')
if not libbpf.found()
    message('net/tap: no RSS support missing libbpf')
    subdir_done()
endif

# Making skeleton needs bpftool
# Debian install this in /usr/sbin which is not in $PATH
bpftool_supports_skel = false
bpftool = find_program('bpftool', '/usr/sbin/bpftool', required: false)
if bpftool.found()
    # Some Ubuntu versions have non-functional bpftool
    bpftool_supports_skel = run_command(bpftool, 'gen', 'help',
                                        check:false).returncode() == 0
endif

if not bpftool_supports_skel
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
machine_name = run_command('uname', '-m', check: true).stdout().strip()
march_include_dir = '/usr/include/' + machine_name + '-linux-gnu'

clang_flags = [
        # these are flags used to build the BPF code
        '-O2',
        '-Wall',
        '-Wextra',
        max_queues,
        '-target',
        'bpf',
        '-g',
        '-c',
]

# Command used to compile BPF pgrograme
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

# Command used to generate header file from BPF object
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
