# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# Test data for autotests

from autotest_test_funcs import *

# groups of tests that can be run in parallel
# the grouping has been found largely empirically
parallel_test_list = [
    {
        "Name":    "Cycles autotest",
        "Command": "cycles_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Timer autotest",
        "Command": "timer_autotest",
        "Func":    timer_autotest,
        "Report":   None,
    },
    {
        "Name":    "Debug autotest",
        "Command": "debug_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Errno autotest",
        "Command": "errno_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Meter autotest",
        "Command": "meter_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Common autotest",
        "Command": "common_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Resource autotest",
        "Command": "resource_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Memory autotest",
        "Command": "memory_autotest",
        "Func":    memory_autotest,
        "Report":  None,
    },
    {
        "Name":    "Read/write lock autotest",
        "Command": "rwlock_autotest",
        "Func":    rwlock_autotest,
        "Report":  None,
    },
    {
        "Name":    "Logs autotest",
        "Command": "logs_autotest",
        "Func":    logs_autotest,
        "Report":  None,
    },
    {
        "Name":    "CPU flags autotest",
        "Command": "cpuflags_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Version autotest",
        "Command": "version_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "EAL filesystem autotest",
        "Command": "eal_fs_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "EAL flags autotest",
        "Command": "eal_flags_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Hash autotest",
        "Command": "hash_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "LPM autotest",
        "Command": "lpm_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "LPM6 autotest",
        "Command": "lpm6_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Memcpy autotest",
        "Command": "memcpy_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Memzone autotest",
        "Command": "memzone_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "String autotest",
        "Command": "string_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Alarm autotest",
        "Command": "alarm_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "PCI autotest",
        "Command": "pci_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Malloc autotest",
        "Command": "malloc_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Multi-process autotest",
        "Command": "multiprocess_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Mbuf autotest",
        "Command": "mbuf_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Per-lcore autotest",
        "Command": "per_lcore_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Ring autotest",
        "Command": "ring_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Spinlock autotest",
        "Command": "spinlock_autotest",
        "Func":    spinlock_autotest,
        "Report":  None,
    },
    {
        "Name":    "Byte order autotest",
        "Command": "byteorder_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "TAILQ autotest",
        "Command": "tailq_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Command-line autotest",
        "Command": "cmdline_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Interrupts autotest",
        "Command": "interrupt_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Function reentrancy autotest",
        "Command": "func_reentrancy_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Mempool autotest",
        "Command": "mempool_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Atomics autotest",
        "Command": "atomic_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Prefetch autotest",
        "Command": "prefetch_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Red autotest",
        "Command": "red_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "PMD ring autotest",
        "Command": "ring_pmd_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Access list control autotest",
        "Command": "acl_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Sched autotest",
        "Command": "sched_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
]

# tests that should not be run when any other tests are running
non_parallel_test_list = [
    {
        "Name":    "Eventdev common autotest",
        "Command": "eventdev_common_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Eventdev sw autotest",
        "Command": "eventdev_sw_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "KNI autotest",
        "Command": "kni_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Mempool performance autotest",
        "Command": "mempool_perf_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Memcpy performance autotest",
        "Command": "memcpy_perf_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":    "Hash performance autotest",
        "Command": "hash_perf_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    {
        "Name":       "Power autotest",
        "Command":    "power_autotest",
        "Func":       default_autotest,
        "Report":      None,
    },
    {
        "Name":       "Power ACPI cpufreq autotest",
        "Command":    "power_acpi_cpufreq_autotest",
        "Func":       default_autotest,
        "Report":     None,
    },
    {
        "Name":       "Power KVM VM  autotest",
        "Command":    "power_kvm_vm_autotest",
        "Func":       default_autotest,
        "Report":     None,
    },
    {
        "Name":    "Timer performance autotest",
        "Command": "timer_perf_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
    #
    # Please always make sure that ring_perf is the last test!
    #
    {
        "Name":    "Ring performance autotest",
        "Command": "ring_perf_autotest",
        "Func":    default_autotest,
        "Report":  None,
    },
]
