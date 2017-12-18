# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if host_machine.system() != 'linux'
	build = false
endif
sources = files('rte_power.c', 'power_acpi_cpufreq.c',
		'power_kvm_vm.c', 'guest_channel.c')
headers = files('rte_power.h')
