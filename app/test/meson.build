# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if not get_option('tests')
    subdir_done()
endif

test_sources = files(
        'commands.c',
        'packet_burst_generator.c',
        'test.c',
        'test_acl.c',
        'test_alarm.c',
        'test_atomic.c',
        'test_barrier.c',
        'test_bitops.c',
        'test_bitmap.c',
        'test_bpf.c',
        'test_byteorder.c',
        'test_cksum.c',
        'test_cksum_perf.c',
        'test_cmdline.c',
        'test_cmdline_cirbuf.c',
        'test_cmdline_etheraddr.c',
        'test_cmdline_ipaddr.c',
        'test_cmdline_lib.c',
        'test_cmdline_num.c',
        'test_cmdline_portlist.c',
        'test_cmdline_string.c',
        'test_common.c',
        'test_cpuflags.c',
        'test_crc.c',
        'test_cryptodev.c',
        'test_cryptodev_asym.c',
        'test_cryptodev_blockcipher.c',
        'test_cryptodev_crosscheck.c',
        'test_cryptodev_security_ipsec.c',
        'test_cryptodev_security_pdcp.c',
        'test_cycles.c',
        'test_debug.c',
        'test_devargs.c',
        'test_distributor.c',
        'test_distributor_perf.c',
        'test_dmadev.c',
        'test_dmadev_api.c',
        'test_eal_flags.c',
        'test_eal_fs.c',
        'test_efd.c',
        'test_efd_perf.c',
        'test_errno.c',
        'test_ethdev_link.c',
        'test_event_crypto_adapter.c',
        'test_event_eth_rx_adapter.c',
        'test_event_ring.c',
        'test_event_timer_adapter.c',
        'test_eventdev.c',
        'test_external_mem.c',
        'test_fbarray.c',
        'test_fib.c',
        'test_fib_perf.c',
        'test_fib6.c',
        'test_fib6_perf.c',
        'test_func_reentrancy.c',
        'test_hash.c',
        'test_hash_functions.c',
        'test_hash_multiwriter.c',
        'test_hash_readwrite.c',
        'test_hash_perf.c',
        'test_hash_readwrite_lf_perf.c',
        'test_interrupts.c',
        'test_ipfrag.c',
        'test_ipsec.c',
        'test_ipsec_sad.c',
        'test_ipsec_perf.c',
        'test_kvargs.c',
        'test_lcores.c',
        'test_logs.c',
        'test_lpm.c',
        'test_lpm6.c',
        'test_lpm6_perf.c',
        'test_lpm_perf.c',
        'test_malloc.c',
        'test_malloc_perf.c',
        'test_mbuf.c',
        'test_member.c',
        'test_member_perf.c',
        'test_memcpy.c',
        'test_memcpy_perf.c',
        'test_memory.c',
        'test_mempool.c',
        'test_mempool_perf.c',
        'test_memzone.c',
        'test_meter.c',
        'test_mcslock.c',
        'test_mp_secondary.c',
        'test_per_lcore.c',
        'test_pflock.c',
        'test_pmd_perf.c',
        'test_power.c',
        'test_power_cpufreq.c',
        'test_power_kvm_vm.c',
        'test_power_intel_uncore.c',
        'test_prefetch.c',
        'test_rand_perf.c',
        'test_rawdev.c',
        'test_rcu_qsbr.c',
        'test_rcu_qsbr_perf.c',
        'test_reassembly_perf.c',
        'test_reciprocal_division.c',
        'test_reciprocal_division_perf.c',
        'test_red.c',
        'test_pie.c',
        'test_reorder.c',
        'test_rib.c',
        'test_rib6.c',
        'test_ring.c',
        'test_ring_mpmc_stress.c',
        'test_ring_hts_stress.c',
        'test_ring_mt_peek_stress.c',
        'test_ring_mt_peek_stress_zc.c',
        'test_ring_perf.c',
        'test_ring_rts_stress.c',
        'test_ring_st_peek_stress.c',
        'test_ring_st_peek_stress_zc.c',
        'test_ring_stress.c',
        'test_rwlock.c',
        'test_sched.c',
        'test_security.c',
        'test_security_inline_macsec.c',
        'test_security_inline_proto.c',
        'test_seqlock.c',
        'test_service_cores.c',
        'test_spinlock.c',
        'test_stack.c',
        'test_stack_perf.c',
        'test_string_fns.c',
        'test_tailq.c',
        'test_thash.c',
        'test_thash_perf.c',
        'test_threads.c',
        'test_timer.c',
        'test_timer_perf.c',
        'test_timer_racecond.c',
        'test_timer_secondary.c',
        'test_ticketlock.c',
        'test_trace.c',
        'test_trace_register.c',
        'test_trace_perf.c',
        'test_version.c',
        'virtual_pmd.c',
)

test_deps = dpdk_libs_enabled
# as well as libs, the pci and vdev bus drivers are needed for a lot of tests
test_deps += ['bus_pci', 'bus_vdev']

# Each test is marked with flags:
# - the first flag indicates whether the test can run in no-huge mode,
# - the second flag indicates whether the test can run with ASan enabled,
fast_tests = [
        ['acl_autotest', true, true],
        ['atomic_autotest', false, true],
        ['bitmap_autotest', true, true],
        ['bpf_autotest', true, true],
        ['bpf_convert_autotest', true, true],
        ['bitops_autotest', true, true],
        ['byteorder_autotest', true, true],
        ['cksum_autotest', true, true],
        ['cmdline_autotest', true, true],
        ['common_autotest', true, true],
        ['cpuflags_autotest', true, true],
        ['debug_autotest', true, true],
        ['devargs_autotest', true, true],
        ['eal_flags_c_opt_autotest', false, false],
        ['eal_flags_main_opt_autotest', false, false],
        ['eal_flags_n_opt_autotest', false, false],
        ['eal_flags_hpet_autotest', false, false],
        ['eal_flags_no_huge_autotest', false, false],
        ['eal_flags_a_opt_autotest', false, false],
        ['eal_flags_b_opt_autotest', false, false],
        ['eal_flags_vdev_opt_autotest', false, false],
        ['eal_flags_r_opt_autotest', false, false],
        ['eal_flags_mem_autotest', false, false],
        ['eal_flags_file_prefix_autotest', false, false],
        ['eal_flags_misc_autotest', false, false],
        ['eal_fs_autotest', true, true],
        ['errno_autotest', true, true],
        ['ethdev_link_status', true, true],
        ['event_ring_autotest', true, true],
        ['fib_autotest', true, true],
        ['fib6_autotest', true, true],
        ['func_reentrancy_autotest', false, true],
        ['hash_autotest', true, true],
        ['interrupt_autotest', true, true],
        ['ipfrag_autotest', false, true],
        ['lcores_autotest', true, true],
        ['logs_autotest', true, true],
        ['lpm_autotest', true, true],
        ['lpm6_autotest', true, true],
        ['malloc_autotest', false, true],
        ['mbuf_autotest', false, true],
        ['mcslock_autotest', false, true],
        ['memcpy_autotest', true, true],
        ['memory_autotest', false, true],
        ['mempool_autotest', false, true],
        ['memzone_autotest', false, true],
        ['meter_autotest', true, true],
        ['multiprocess_autotest', false, false],
        ['per_lcore_autotest', true, true],
        ['pflock_autotest', true, true],
        ['prefetch_autotest', true, true],
        ['rcu_qsbr_autotest', true, true],
        ['pie_autotest', true, true],
        ['rib_autotest', true, true],
        ['rib6_autotest', true, true],
        ['ring_autotest', true, true],
        ['rwlock_test1_autotest', true, true],
        ['rwlock_rda_autotest', true, true],
        ['rwlock_rds_wrm_autotest', true, true],
        ['rwlock_rde_wro_autotest', true, true],
        ['sched_autotest', true, true],
        ['security_autotest', false, true],
        ['seqlock_autotest', true, true],
        ['spinlock_autotest', true, true],
        ['stack_autotest', false, true],
        ['stack_lf_autotest', false, true],
        ['string_autotest', true, true],
        ['tailq_autotest', true, true],
        ['ticketlock_autotest', true, true],
        ['timer_autotest', false, true],
        ['user_delay_us', true, true],
        ['version_autotest', true, true],
        ['crc_autotest', true, true],
        ['distributor_autotest', false, true],
        ['eventdev_common_autotest', true, true],
        ['fbarray_autotest', true, true],
        ['hash_readwrite_func_autotest', false, true],
        ['ipsec_autotest', true, true],
        ['kvargs_autotest', true, true],
        ['member_autotest', true, true],
        ['power_cpufreq_autotest', false, true],
        ['power_autotest', true, true],
        ['power_kvm_vm_autotest', false, true],
        ['power_intel_uncore_autotest', true, true],
        ['reorder_autotest', true, true],
        ['service_autotest', true, true],
        ['thash_autotest', true, true],
        ['threads_autotest', true, true],
        ['trace_autotest', true, true],
]

# Tests known to have issues or which don't belong in other tests lists.
extra_test_names = [
        'alarm_autotest', # ee00af60170b ("test: remove strict timing requirements some tests")
        'red_autotest', # https://bugs.dpdk.org/show_bug.cgi?id=826
]

perf_test_names = [
        'ring_perf_autotest',
        'malloc_perf_autotest',
        'mempool_perf_autotest',
        'memcpy_perf_autotest',
        'hash_perf_autotest',
        'timer_perf_autotest',
        'reciprocal_division',
        'reciprocal_division_perf',
        'lpm_perf_autotest',
        'rib_slow_autotest',
        'fib_slow_autotest',
        'fib_perf_autotest',
        'red_all',
        'pie_all',
        'barrier_autotest',
        'hash_multiwriter_autotest',
        'timer_racecond_autotest',
        'efd_autotest',
        'hash_functions_autotest',
        'member_perf_autotest',
        'efd_perf_autotest',
        'lpm6_perf_autotest',
        'rib6_slow_autotest',
        'fib6_slow_autotest',
        'fib6_perf_autotest',
        'rcu_qsbr_perf_autotest',
        'red_perf',
        'pie_perf',
        'distributor_perf_autotest',
        'pmd_perf_autotest',
        'service_perf_autotest',
        'stack_perf_autotest',
        'stack_lf_perf_autotest',
        'rand_perf_autotest',
        'hash_readwrite_perf_autotest',
        'hash_readwrite_lf_perf_autotest',
        'trace_perf_autotest',
        'ipsec_perf_autotest',
        'thash_perf_autotest',
        'reassembly_perf_autotest',
]

driver_test_names = [
        'cryptodev_aesni_gcm_autotest',
        'cryptodev_aesni_mb_autotest',
        'cryptodev_chacha_poly_mb_autotest',
        'cryptodev_cn10k_autotest',
        'cryptodev_cn9k_autotest',
        'cryptodev_cpu_aesni_mb_autotest',
        'cryptodev_cpu_aesni_gcm_autotest',
        'cryptodev_dpaa2_sec_autotest',
        'cryptodev_dpaa_sec_autotest',
        'cryptodev_null_autotest',
        'cryptodev_openssl_autotest',
        'cryptodev_qat_autotest',
        'cryptodev_qat_asym_autotest',
        'cryptodev_qat_raw_api_autotest',
        'cryptodev_sw_armv8_autotest',
        'cryptodev_sw_kasumi_autotest',
        'cryptodev_sw_mvsam_autotest',
        'cryptodev_sw_snow3g_autotest',
        'cryptodev_sw_zuc_autotest',
        'cryptodev_uadk_autotest',
        'dmadev_autotest',
]

dump_test_names = []

if not is_windows
    driver_test_names += [
            'cryptodev_openssl_asym_autotest',
            'eventdev_selftest_octeontx',
            'eventdev_selftest_sw',
    ]

    dump_test_names += [
            'dump_struct_sizes',
            'dump_mempool',
            'dump_malloc_stats',
            'dump_devargs',
            'dump_log_types',
            'dump_ring',
            'dump_physmem',
            'dump_memzone',
    ]
endif

# The following linkages are an exception to allow running the
# unit tests without requiring that the developer install the
# DPDK libraries.  Explicit linkage of drivers (plugin libraries)
# in applications should not be used.
if dpdk_conf.has('RTE_MEMPOOL_RING')
    test_deps += 'mempool_ring'
endif
if dpdk_conf.has('RTE_MEMPOOL_STACK')
    test_deps += 'mempool_stack'
endif
if dpdk_conf.has('RTE_EVENT_SKELETON')
    test_deps += 'event_skeleton'
endif

if dpdk_conf.has('RTE_LIB_GRAPH')
    test_sources += 'test_graph.c'
    fast_tests += [['graph_autotest', true, true]]
    fast_tests += [['node_list_dump', true, true]]
    test_sources += 'test_graph_perf.c'
    perf_test_names += 'graph_perf_autotest'
endif
if dpdk_conf.has('RTE_LIB_METRICS')
    test_sources += ['test_metrics.c']
    fast_tests += [['metrics_autotest', true, true]]
endif
if not is_windows and dpdk_conf.has('RTE_LIB_TELEMETRY')
    test_sources += ['test_telemetry_json.c', 'test_telemetry_data.c']
    fast_tests += [['telemetry_json_autotest', true, true]]
    fast_tests += [['telemetry_data_autotest', true, true]]
endif
if dpdk_conf.has('RTE_LIB_PIPELINE')
# pipeline lib depends on port and table libs, so those must be present
# if pipeline library is.
    test_sources += [
            'test_table.c',
            'test_table_acl.c',
            'test_table_combined.c',
            'test_table_pipeline.c',
            'test_table_ports.c',
            'test_table_tables.c',
    ]
    fast_tests += [['table_autotest', true, true]]
endif

# The following linkages of drivers are required because
# they are used via a driver-specific API.
if dpdk_conf.has('RTE_NET_BOND')
    test_deps += 'net_bond'
    test_sources += ['test_link_bonding.c', 'test_link_bonding_rssconf.c']
    driver_test_names += ['link_bonding_autotest', 'link_bonding_rssconf_autotest']
    if dpdk_conf.has('RTE_NET_RING')
        test_sources += 'test_link_bonding_mode4.c'
        driver_test_names += 'link_bonding_mode4_autotest'
    endif
endif
if dpdk_conf.has('RTE_LIB_EVENTDEV') and dpdk_conf.has('RTE_NET_RING')
    test_deps += 'net_ring'
    test_sources += 'test_pmd_ring_perf.c'
    test_sources += 'test_pmd_ring.c'
    test_sources += 'test_event_eth_tx_adapter.c'
    test_sources += 'sample_packet_forward.c'
    fast_tests += [['ring_pmd_autotest', true, true]]
    perf_test_names += 'ring_pmd_perf_autotest'
    fast_tests += [['event_eth_tx_adapter_autotest', false, true]]
    if dpdk_conf.has('RTE_LIB_BITRATESTATS')
        test_sources += 'test_bitratestats.c'
        fast_tests += [['bitratestats_autotest', true, true]]
    endif
    if dpdk_conf.has('RTE_LIB_LATENCYSTATS')
        test_sources += 'test_latencystats.c'
        fast_tests += [['latencystats_autotest', true, true]]
    endif
    if dpdk_conf.has('RTE_LIB_PDUMP')
        test_sources += 'test_pdump.c'
        fast_tests += [['pdump_autotest', true, false]]
    endif
endif
if dpdk_conf.has('RTE_NET_NULL')
    test_deps += 'net_null'
    test_sources += 'test_vdev.c'
    fast_tests += [['vdev_autotest', true, true]]
endif
if dpdk_conf.has('RTE_RAW_SKELETON')
    test_deps += 'raw_skeleton'
    fast_tests += [['rawdev_autotest', true, true]]
endif

if dpdk_conf.has('RTE_HAS_LIBPCAP')
    ext_deps += pcap_dep
    if dpdk_conf.has('RTE_LIB_PCAPNG')
        test_sources += 'test_pcapng.c'
    endif
endif

if dpdk_conf.has('RTE_LIB_PDCP')
    test_sources += 'test_pdcp.c'
    fast_tests += [['pdcp_autotest', false, true]]
endif

if cc.has_argument('-Wno-format-truncation')
    cflags += '-Wno-format-truncation'
endif

# Strict-aliasing rules are violated by uint8_t[] to context size casts.
cflags += '-fno-strict-aliasing'

# Enable using internal APIs in unit tests
cflags += ['-DALLOW_INTERNAL_API']

test_dep_objs = []
if dpdk_conf.has('RTE_LIB_COMPRESSDEV')
    compress_test_dep = dependency('zlib', required: false, method: 'pkg-config')
    if compress_test_dep.found()
        test_dep_objs += compress_test_dep
        test_sources += 'test_compressdev.c'
        fast_tests += [['compressdev_autotest', false, true]]
    endif
endif

if dpdk_conf.has('RTE_CRYPTO_SCHEDULER')
    driver_test_names += 'cryptodev_scheduler_autotest'
    test_deps += 'crypto_scheduler'
endif

foreach d:test_deps
    def_lib = get_option('default_library')
    test_dep_objs += get_variable(def_lib + '_rte_' + d)
endforeach

link_libs = []
if get_option('default_library') == 'static'
    link_libs = dpdk_static_libraries + dpdk_drivers
endif

dpdk_test = executable('dpdk-test',
        test_sources,
        link_whole: link_libs,
        dependencies: test_dep_objs + ext_deps,
        c_args: cflags,
        install_rpath: join_paths(get_option('prefix'),
             driver_install_path),
        install: true)

has_hugepage = run_command(py3, files('has_hugepage.py'), check: true).stdout().strip() != '0'
message('hugepage availability: @0@'.format(has_hugepage))

# some perf tests (eg: memcpy perf autotest)take very long
# to complete, so timeout to 10 minutes
timeout_seconds = 600
timeout_seconds_fast = 10

test_no_huge_args = ['--no-huge', '-m', '2048']

foreach arg : fast_tests
    test_args = []
    run_test = true
    if not has_hugepage
        if arg[1]
            test_args += test_no_huge_args
        else
            run_test = false
        endif
    endif

    if get_option('b_sanitize') == 'address' or get_option('b_sanitize') == 'address,undefined'
        if not arg[2]
            run_test = false
        endif
    endif

    if (get_option('default_library') == 'shared' and
        arg[0] == 'event_eth_tx_adapter_autotest')
        test_args += ['-d', dpdk_drivers_build_dir]
    endif
    if is_linux
        test_args += ['--file-prefix=@0@'.format(arg[0])]
    endif

    if run_test
        test(arg[0], dpdk_test,
                env : ['DPDK_TEST=' + arg[0]],
                args : test_args,
                timeout : timeout_seconds_fast,
                is_parallel : false,
                suite : 'fast-tests')
        if not is_windows and arg[0] == 'trace_autotest'
            test_args += ['--trace=.*']
            test_args += ['--trace-dir=@0@'.format(meson.current_build_dir())]
            test(arg[0] + '_with_traces', dpdk_test,
                    env : ['DPDK_TEST=' + arg[0]],
                    args : test_args,
                    timeout : timeout_seconds_fast,
                    is_parallel : false,
                    suite : 'fast-tests')
        endif
    endif
endforeach

if not is_windows and dpdk_conf.has('RTE_LIB_TELEMETRY')
    test_args = [dpdk_test]
    test_args += test_no_huge_args
    if get_option('default_library') == 'shared'
        test_args += ['-d', dpdk_drivers_build_dir]
    endif
    if dpdk_conf.has('RTE_CRYPTO_NULL')
        test_args += ['--vdev=crypto_null0']
    endif
    if dpdk_conf.has('RTE_DMA_SKELETON')
        test_args += ['--vdev=dma_skeleton0']
    endif
    if dpdk_conf.has('RTE_EVENT_SKELETON')
        test_args += ['--vdev=event_skeleton0']
    endif
    if dpdk_conf.has('RTE_NET_NULL')
        test_args += ['--vdev=net_null0']
    endif
    if dpdk_conf.has('RTE_RAW_SKELETON')
        test_args += ['--vdev=rawdev_skeleton0']
    endif
    test_args += ['-a', '0000:00:00.0']
    test('telemetry_all', find_program('test_telemetry.sh'),
            args: test_args,
            timeout : timeout_seconds_fast,
            is_parallel : false,
            suite : 'fast-tests')
endif

foreach arg : perf_test_names
    test(arg, dpdk_test,
            env : ['DPDK_TEST=' + arg],
            timeout : timeout_seconds,
            is_parallel : false,
            suite : 'perf-tests')
endforeach

foreach arg : driver_test_names
    test(arg, dpdk_test,
            env : ['DPDK_TEST=' + arg],
            timeout : timeout_seconds,
            is_parallel : false,
            suite : 'driver-tests')
endforeach

foreach arg : dump_test_names
    test(arg, dpdk_test,
            env : ['DPDK_TEST=' + arg],
            timeout : timeout_seconds,
            is_parallel : false,
            suite : 'debug-tests')
endforeach

foreach arg : extra_test_names
    test(arg, dpdk_test,
            env : ['DPDK_TEST=' + arg],
            timeout : timeout_seconds,
            is_parallel : false,
            suite : 'extra-tests')
endforeach
