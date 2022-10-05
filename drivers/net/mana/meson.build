# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Microsoft Corporation

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86_64')
    build = false
    reason = 'mana is supported on Linux X86_64'
    subdir_done()
endif

deps += ['pci', 'bus_pci', 'net', 'eal', 'kvargs']

sources += files(
        'gdma.c',
        'mana.c',
        'mp.c',
        'mr.c',
        'tx.c',
)

libnames = ['ibverbs', 'mana' ]
foreach libname:libnames
    lib = cc.find_library(libname, required:false)
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
