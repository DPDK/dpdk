# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Stephen Hemminger <stephen@networkplumber.org>

bpf_test_hdrs = [ ]

# use clang to compile to BPF
clang_supports_bpf = false
clang = find_program('clang', required: false)
if clang.found()
    clang_supports_bpf = run_command(clang, '-target', 'bpf', '--print-supported-cpus',
                                     check: false).returncode() == 0
endif

if not clang_supports_bpf
    message('app/test_bpf: no BPF load tests missing clang BPF support')
    subdir_done()

endif

xxd = find_program('xxd', required: false)
if not xxd.found()
    message('app/test_bpf: missing xxd required to convert object to array')
    subdir_done()
endif

# BPF compiler flags
bpf_cflags = [ '-O2', '-target', 'bpf', '-g', '-c']

# Enable test in test_bpf.c
cflags += '-DTEST_BPF_ELF_LOAD'

# BPF sources to compile
bpf_progs = {
    'load': 'test_bpf_load',
}

foreach bpf_src, bpf_hdr: bpf_progs
    # Compile BPF C source to object file
    bpf_obj = custom_target(bpf_src + '_o',
        input: bpf_src + '.c',
        output: bpf_src + '.o',
        command: [ clang, bpf_cflags, '@INPUT@', '-o', '@OUTPUT@'])

    # Convert object file to C header using xxd
    bpf_test_h = custom_target(bpf_src + '_h',
        input: bpf_obj,
        output: bpf_hdr + '.h',
        command: [ xxd, '-i', '@INPUT@', '@OUTPUT@'])

    resources += bpf_test_h

endforeach
