# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

sources = [
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
]


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
c_args = cflags
foreach flag: error_cflags
        if cc.has_argument(flag)
                c_args += flag
        endif
endforeach

base_lib = static_library('qede_base', sources,
	dependencies: static_rte_net,
	c_args: c_args)
base_objs = base_lib.extract_all_objects()
