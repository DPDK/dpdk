/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation.
 */

/** This file contains a list of EAL commandline arguments.
 *
 * It's designed to be included multiple times in the codebase to
 * generate both argument structure and the argument definitions
 * for argparse.
 */

/* check that all ARG macros are defined */
#ifndef LIST_ARG
#error "LIST_ARG macro must be defined before including " __FILE__
#endif
#ifndef STR_ARG
#error "STR_ARG macro must be defined before including " __FILE__
#endif
#ifndef OPT_STR_ARG
#error "OPT_STR_ARG macro must be defined before including " __FILE__
#endif
#ifndef BOOL_ARG
#error "BOOL_ARG macro must be defined before including " __FILE__
#endif
#ifndef STR_ALIAS
#error "STR_ALIAS macro must be defined before including " __FILE__
#endif


/*
 * list of EAL arguments as struct rte_argparse_arg.
 * Format of each entry: long name, short name, help string, struct member name.
 */
/* (Alphabetical) List of common options first */
LIST_ARG("--allow", "-a", "Add device to allow-list, causing DPDK to only use specified devices", allow)
STR_ARG("--base-virtaddr", NULL, "Base virtual address to reserve memory", base_virtaddr)
LIST_ARG("--block", "-b", "Add device to block-list, preventing DPDK from using the device", block)
STR_ARG("--coremask", "-c", "[Deprecated] Hexadecimal bitmask of cores to use", coremask)
LIST_ARG("--driver-path", "-d", "Path to external driver shared object, or directory of drivers", driver_path)
STR_ARG("--force-max-simd-bitwidth", NULL, "Set max SIMD bitwidth to use in vector code paths", force_max_simd_bitwidth)
OPT_STR_ARG("--huge-unlink", NULL, "Unlink hugetlbfs files on exit (existing|always|never)", huge_unlink)
BOOL_ARG("--in-memory", NULL, "DPDK should not create shared mmap files in filesystem (disables secondary process support)", in_memory)
STR_ARG("--iova-mode", NULL, "IOVA mapping mode, physical (pa)/virtual (va)", iova_mode)
STR_ARG("--lcores", "-l", "List of CPU cores to use", lcores)
BOOL_ARG("--legacy-mem", NULL, "Enable legacy memory behavior", legacy_mem)
OPT_STR_ARG("--log-color", NULL, "Enable/disable color in log output", log_color)
LIST_ARG("--log-level", NULL, "Log level for loggers; use log-level=help for list of log types and levels", log_level)
OPT_STR_ARG("--log-timestamp", NULL, "Enable/disable timestamp in log output", log_timestamp)
STR_ARG("--main-lcore", NULL, "Select which core to use for the main thread", main_lcore)
STR_ARG("--mbuf-pool-ops-name", NULL, "User defined mbuf default pool ops name", mbuf_pool_ops_name)
STR_ARG("--memory-channels", "-n", "Number of memory channels per socket", memory_channels)
STR_ARG("--memory-ranks", "-r", "Force number of memory ranks (don't detect)", memory_ranks)
STR_ARG("--memory-size", "-m", "Total size of memory to allocate initially", memory_size)
BOOL_ARG("--no-hpet", NULL, "Disable HPET timer", no_hpet)
BOOL_ARG("--no-huge", NULL, "Disable hugetlbfs support", no_huge)
BOOL_ARG("--no-pci", NULL, "Disable all PCI devices", no_pci)
BOOL_ARG("--no-shconf", NULL, "Disable shared config file generation", no_shconf)
BOOL_ARG("--no-telemetry", NULL, "Disable telemetry", no_telemetry)
STR_ARG("--proc-type", NULL, "Type of process (primary|secondary|auto)", proc_type)
STR_ARG("--service-corelist", "-S", "List of cores to use for service threads", service_corelist)
STR_ARG("--service-coremask", "-s", "[Deprecated] Bitmask of cores to use for service threads", service_coremask)
BOOL_ARG("--single-file-segments", NULL, "Store all pages within single files (per-page-size, per-node)", single_file_segments)
BOOL_ARG("--telemetry", NULL, "Enable telemetry", telemetry)
LIST_ARG("--vdev", NULL, "Add a virtual device to the system; format=<driver><id>[,key=val,...]", vdev)
BOOL_ARG("--vmware-tsc-map", NULL, "Use VMware TSC mapping instead of native RDTSC", vmware_tsc_map)
BOOL_ARG("--version", "-v", "Show version", version)

#if defined(INCLUDE_ALL_ARG) || !defined(RTE_EXEC_ENV_WINDOWS)
/* Linux and FreeBSD options*/
OPT_STR_ARG("--syslog", NULL, "Log to syslog (and optionally set facility)", syslog)
LIST_ARG("--trace", NULL, "Enable trace based on regular expression trace name", trace)
STR_ARG("--trace-bufsz", NULL, "Trace buffer size", trace_bufsz)
STR_ARG("--trace-dir", NULL, "Trace directory", trace_dir)
STR_ARG("--trace-mode", NULL, "Trace mode", trace_mode)
#endif

#if defined(INCLUDE_ALL_ARG) || defined(RTE_EXEC_ENV_LINUX)
/* Linux-only options */
BOOL_ARG("--create-uio-dev", NULL, "Create /dev/uioX devices", create_uio_dev)
STR_ARG("--file-prefix", NULL, "Base filename of hugetlbfs files", file_prefix)
STR_ARG("--huge-dir", NULL, "Directory for hugepage files", huge_dir)
OPT_STR_ARG("--huge-worker-stack", NULL, "Allocate worker thread stacks from hugepage memory, with optional size (kB)", huge_worker_stack)
BOOL_ARG("--match-allocations", NULL, "Free hugepages exactly as allocated", match_allocations)
STR_ARG("--numa-mem", NULL, "Memory to allocate on NUMA nodes (comma separated values)", numa_mem)
STR_ARG("--numa-limit", NULL, "Limit memory allocation on NUMA nodes (comma separated values)", numa_limit)
STR_ALIAS("--socket-mem", NULL, "Alias for --numa-mem", numa_mem)
STR_ALIAS("--socket-limit", NULL, "Alias for --numa-limit", numa_limit)
STR_ARG("--vfio-intr", NULL, "VFIO interrupt mode (legacy|msi|msix)", vfio_intr)
STR_ARG("--vfio-vf-token", NULL, "VF token (UUID) shared between SR-IOV PF and VFs", vfio_vf_token)
#endif

/* undefine all used defines */
#undef LIST_ARG
#undef STR_ARG
#undef OPT_STR_ARG
#undef BOOL_ARG
#undef STR_ALIAS
