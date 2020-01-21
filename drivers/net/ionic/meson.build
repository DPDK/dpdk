# SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
# Copyright(c) 2019 Pensando

sources = files(
	'ionic_mac_api.c',
	'ionic_rx_filter.c',
	'ionic_rxtx.c',
	'ionic_dev.c',
	'ionic_ethdev.c',
	'ionic_lif.c',
	'ionic_main.c'
)

error_cflags = [
  '-Wno-missing-field-initializers',
]

foreach flag: error_cflags
        if cc.has_argument(flag)
                c_args += flag
        endif
endforeach
