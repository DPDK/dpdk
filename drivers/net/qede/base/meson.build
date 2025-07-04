# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

base_sources = files(
        'bcm_osal.c',
        'ecore_cxt.c',
        'ecore_dcbx.c',
        'ecore_dev.c',
        'ecore_hw.c',
        'ecore_init_fw_funcs.c',
        'ecore_init_ops.c',
        'ecore_int.c',
        'ecore_l2.c',
        'ecore_mcp.c',
        'ecore_sp_commands.c',
        'ecore_spq.c',
        'ecore_sriov.c',
        'ecore_vf.c',
)

if is_ms_compiler
        error_cflags = [
                '/wd4101', # unreferenced local variable
        ]
else
        error_cflags = [
                '-Wno-unused-parameter',
                '-Wno-sign-compare',
                '-Wno-missing-prototypes',
                '-Wno-cast-qual',
                '-Wno-unused-function',
                '-Wno-unused-variable',
                '-Wno-strict-aliasing',
                '-Wno-missing-prototypes',
                '-Wno-unused-value',
                '-Wno-format-nonliteral',
                '-Wno-shift-negative-value',
                '-Wno-unused-but-set-variable',
                '-Wno-missing-declarations',
                '-Wno-maybe-uninitialized',
                '-Wno-strict-prototypes',
                '-Wno-shift-negative-value',
                '-Wno-implicit-fallthrough',
                '-Wno-format-extra-args',
                '-Wno-visibility',
                '-Wno-empty-body',
                '-Wno-invalid-source-encoding',
                '-Wno-sometimes-uninitialized',
                '-Wno-pointer-bool-conversion',
        ]
endif

foreach flag: error_cflags
    if cc.has_argument(flag)
        base_cflags += flag
    endif
endforeach
