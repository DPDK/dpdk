# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_eth_af_xdp.c')

xdp_dep = dependency('libxdp', version : '>=1.2.2', required: false, method: 'pkg-config')
bpf_dep = dependency('libbpf', required: false, method: 'pkg-config')
if not bpf_dep.found()
    bpf_dep = cc.find_library('bpf', required: false)
endif

if cc.has_header('linux/if_xdp.h')
    if xdp_dep.found() and cc.has_header('xdp/xsk.h')
        if bpf_dep.found() and cc.has_header('bpf/bpf.h')
            cflags += ['-DRTE_NET_AF_XDP_LIBXDP']
            cflags += ['-DRTE_NET_AF_XDP_SHARED_UMEM']
            ext_deps += xdp_dep
            ext_deps += bpf_dep
            bpf_ver_dep = dependency('libbpf', version : '>=0.7.0',
                                 required: false, method: 'pkg-config')
            if bpf_ver_dep.found()
                cflags += ['-DRTE_NET_AF_XDP_LIBBPF_OBJ_OPEN']
            endif
        else
            build = false
            reason = 'missing dependency, libbpf'
        endif
    elif bpf_dep.found() and cc.has_header('bpf/xsk.h') and cc.has_header('bpf/bpf.h')
        # libxdp not found. Rely solely on libbpf for xsk functionality
        # which is only available in versions <= v0.6.0.
        bpf_ver_dep = dependency('libbpf', version : '<=0.6.0',
                                 required: false, method: 'pkg-config')
        if bpf_ver_dep.found()
            ext_deps += bpf_dep
            bpf_shumem_ver_dep = dependency('libbpf', version : '>=0.2.0',
                            required: false, method: 'pkg-config')
            if bpf_shumem_ver_dep.found()
                cflags += ['-DRTE_NET_AF_XDP_SHARED_UMEM']
            endif
        else
            build = false
            reason = 'missing dependency, "libxdp" or "libbpf <= v0.6.0"'
        endif
    else
        build = false
        reason = 'missing dependency, "libxdp" and "libbpf"'
    endif
else
    build = false
    reason = 'missing header, "linux/if_xdp.h"'
endif
