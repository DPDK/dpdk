# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Microsoft Corporation

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86')
    build = false
    reason = 'only supported on x86 Linux'
    subdir_done()
endif

deps += ['pci', 'bus_pci', 'net', 'eal', 'kvargs']

sources += files(
        'gdma.c',
        'mana.c',
        'mp.c',
        'mr.c',
        'rx.c',
        'tx.c',
)

libnames = ['ibverbs', 'mana']
foreach libname:libnames
    lib = dependency('lib' + libname, required:false)
    if not lib.found()
        lib = cc.find_library(libname, required:false)
    endif
    if lib.found()
        ext_deps += lib
    else
        build = false
        reason = 'missing dependency, "' + libname + '"'
        subdir_done()
    endif
endforeach

required_symbols = [
    ['infiniband/manadv.h', 'manadv_set_context_attr'],
    ['infiniband/manadv.h', 'manadv_init_obj'],
    ['infiniband/manadv.h', 'MANADV_CTX_ATTR_BUF_ALLOCATORS'],
    ['infiniband/manadv.h', 'MANADV_OBJ_QP'],
    ['infiniband/manadv.h', 'MANADV_OBJ_CQ'],
    ['infiniband/manadv.h', 'MANADV_OBJ_RWQ'],
]

foreach arg:required_symbols
    if not cc.has_header_symbol(arg[0], arg[1])
        build = false
        reason = 'missing symbol "' + arg[1] + '" in "' + arg[0] + '"'
        subdir_done()
    endif
endforeach
