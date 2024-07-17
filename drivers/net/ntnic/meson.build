# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020-2023 Napatech A/S

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86_64')
        build = false
        reason = 'only supported on x86_64 Linux'
        subdir_done()
endif

# includes
includes = [
        include_directories('.'),
        include_directories('include'),
        include_directories('ntlog'),
        include_directories('ntutil'),
        include_directories('nthw/core/include'),
        include_directories('nthw'),
        include_directories('nthw/supported'),
        include_directories('nthw/model'),
        include_directories('nim/'),
]

# all sources
sources = files(
        'adapter/nt4ga_adapter.c',
        'link_mgmt/link_100g/nt4ga_link_100g.c',
        'link_mgmt/nt4ga_link.c',
        'nim/i2c_nim.c',
        'nthw/supported/nthw_fpga_9563_055_039_0000.c',
        'nthw/supported/nthw_fpga_instances.c',
        'nthw/supported/nthw_fpga_mod_str_map.c',
        'nthw/core/nt200a0x/clock_profiles/nthw_fpga_clk9563.c',
        'nthw/core/nt200a0x/nthw_fpga_nt200a0x.c',
        'nthw/core/nt200a0x/reset/nthw_fpga_rst9563.c',
        'nthw/core/nt200a0x/reset/nthw_fpga_rst_nt200a0x.c',
        'nthw/core/nthw_fpga.c',
        'nthw/core/nthw_gpio_phy.c',
        'nthw/core/nthw_hif.c',
        'nthw/core/nthw_i2cm.c',
        'nthw/core/nthw_iic.c',
        'nthw/core/nthw_mac_pcs.c',
        'nthw/core/nthw_pcie3.c',
        'nthw/core/nthw_sdc.c',
        'nthw/core/nthw_si5340.c',
        'nthw/model/nthw_fpga_model.c',
        'nthw/nthw_platform.c',
        'nthw/nthw_rac.c',
        'ntlog/ntlog.c',
        'ntutil/nt_util.c',
        'ntnic_mod_reg.c',
        'ntnic_vfio.c',
        'ntnic_ethdev.c',
)
