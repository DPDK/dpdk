# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_eth_af_xdp.c')

libxdp_ver = '>=1.2.2'
xdp_dep = dependency('libxdp', version : libxdp_ver, required: false, method: 'pkg-config')
bpf_dep = dependency('libbpf', required: false, method: 'pkg-config')
if not bpf_dep.found()
    bpf_dep = cc.find_library('bpf', required: false)
endif

if cc.has_header('linux/if_xdp.h')
    if xdp_dep.found() and cc.has_header('xdp/xsk.h')
        cflags += ['-DRTE_NET_AF_XDP_LIBXDP']
        ext_deps += xdp_dep
        if bpf_dep.found() and cc.has_header('bpf/bpf.h')
            ext_deps += bpf_dep
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
        else
            build = false
            reason = 'missing dependency, "libxdp ' + libxdp_ver + '" or "libbpf <= v0.6.0"'
        endif
    else
        build = false
        reason = 'missing dependency, "libxdp ' + libxdp_ver + '" and "libbpf"'
    endif
else
    build = false
    reason = 'missing header, "linux/if_xdp.h"'
endif

if build
  xsk_check_prefix = '''
#ifndef typeof
#define typeof __typeof__
#endif

#ifndef asm
#define asm __asm__
#endif

#ifdef RTE_NET_AF_XDP_LIBXDP
#include <xdp/xsk.h>
#else
#include <bpf/xsk.h>
#endif
  '''

  if cc.has_function('xsk_socket__create_shared', prefix : xsk_check_prefix,
                     dependencies : ext_deps, args: cflags)
      cflags += ['-DRTE_NET_AF_XDP_SHARED_UMEM']
  endif
  if cc.has_function('bpf_object__next_program',
                     prefix : '#include <bpf/libbpf.h>',
                     dependencies : bpf_dep, args: cflags)
      cflags += ['-DRTE_NET_AF_XDP_LIBBPF_OBJ_OPEN']
  endif
  if cc.has_function('bpf_xdp_attach',
                     prefix : '#include <bpf/libbpf.h>',
                     dependencies : bpf_dep, args: cflags)
      cflags += ['-DRTE_NET_AF_XDP_LIBBPF_XDP_ATTACH']
  endif
endif

require_iova_in_mbuf = false
