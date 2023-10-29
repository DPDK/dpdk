# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 NVIDIA Corporation & Affiliates

if not (is_linux and mlx5_config.get('HAVE_IBV_FLOW_DV_SUPPORT', false))
    subdir_done()
endif

includes += include_directories('.')
sources += files(
        'mlx5dr_context.c',
        'mlx5dr_table.c',
        'mlx5dr_matcher.c',
        'mlx5dr_rule.c',
        'mlx5dr_action.c',
        'mlx5dr_buddy.c',
        'mlx5dr_pool.c',
        'mlx5dr_cmd.c',
        'mlx5dr_send.c',
        'mlx5dr_definer.c',
        'mlx5dr_debug.c',
        'mlx5dr_pat_arg.c',
        'mlx5dr_crc32.c',
)
