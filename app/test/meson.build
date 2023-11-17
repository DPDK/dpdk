# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2023 Intel Corporation

# the main test files [test.c and commands.c] relies on these libraries
deps += ['cmdline', 'ring', 'mempool', 'mbuf']
sources += files('commands.c', 'test.c')

# optional dependencies: some files may use these - and so we should link them in -
# but do not explicitly require them so they are not listed in the per-file lists below
optional_deps = ['crypto_scheduler']

# some other utility C files, providing functions used by various tests
# so we need to include these deps in the dependency list for the files using those fns.
packet_burst_generator_deps = ['net']
sample_packet_forward_deps = ['net_ring', 'ethdev', 'bus_vdev']
virtual_pmd_deps = ['ethdev', 'net', 'bus_pci']
# test_cryptodev has material that other crypto tests need
test_cryptodev_deps = ['bus_vdev', 'net', 'cryptodev', 'security']

source_file_deps = {
    # The C files providing functionality to other test cases
    'packet_burst_generator.c': packet_burst_generator_deps,
#    'resource.c': [],          # unused currently.
    'sample_packet_forward.c': sample_packet_forward_deps,
    'virtual_pmd.c': virtual_pmd_deps,

    # the various test_*.c files
    'test_acl.c': ['net', 'acl'],
    'test_alarm.c': [],
    'test_atomic.c': ['hash'],
    'test_barrier.c': [],
    'test_bitcount.c': [],
    'test_bitmap.c': [],
    'test_bitops.c': [],
    'test_bitratestats.c': ['metrics', 'bitratestats', 'ethdev'] + sample_packet_forward_deps,
    'test_bpf.c': ['bpf', 'net'],
    'test_byteorder.c': [],
#    'test_cfgfile.c': ['cfgfile'],
    'test_cksum.c': ['net'],
    'test_cksum_perf.c': ['net'],
    'test_cmdline.c': [],
    'test_cmdline_cirbuf.c': [],
    'test_cmdline_etheraddr.c': ['net'],
    'test_cmdline_ipaddr.c': [],
    'test_cmdline_lib.c': [],
    'test_cmdline_num.c': [],
    'test_cmdline_portlist.c': [],
    'test_cmdline_string.c': [],
    'test_common.c': [],
    'test_compressdev.c': ['compressdev'],
    'test_cpuflags.c': [],
    'test_crc.c': ['net'],
    'test_cryptodev.c': test_cryptodev_deps,
    'test_cryptodev_asym.c': ['bus_vdev'] + test_cryptodev_deps,
    'test_cryptodev_blockcipher.c': test_cryptodev_deps,
    'test_cryptodev_crosscheck.c': test_cryptodev_deps,
    'test_cryptodev_security_ipsec.c': test_cryptodev_deps,
    'test_cryptodev_security_pdcp.c': test_cryptodev_deps,
    'test_cycles.c': [],
    'test_debug.c': [],
    'test_devargs.c': ['kvargs'],
    'test_dispatcher.c': ['dispatcher'],
    'test_distributor.c': ['distributor'],
    'test_distributor_perf.c': ['distributor'],
    'test_dmadev.c': ['dmadev', 'bus_vdev'],
    'test_dmadev_api.c': ['dmadev'],
    'test_eal_flags.c': [],
    'test_eal_fs.c': [],
    'test_efd.c': ['efd', 'net'],
    'test_efd_perf.c': ['efd', 'hash'],
    'test_errno.c': [],
    'test_ethdev_api.c': ['ethdev'],
    'test_ethdev_link.c': ['ethdev'],
    'test_event_crypto_adapter.c': ['cryptodev', 'eventdev', 'bus_vdev'],
    'test_event_dma_adapter.c': ['dmadev', 'eventdev', 'bus_vdev'],
    'test_event_eth_rx_adapter.c': ['ethdev', 'eventdev', 'bus_vdev'],
    'test_event_eth_tx_adapter.c': ['bus_vdev', 'ethdev', 'net_ring', 'eventdev'],
    'test_event_ring.c': ['eventdev'],
    'test_event_timer_adapter.c': ['ethdev', 'eventdev', 'bus_vdev'],
    'test_eventdev.c': ['eventdev', 'bus_vdev'],
    'test_external_mem.c': [],
    'test_fbarray.c': [],
    'test_fib.c': ['net', 'fib'],
    'test_fib6.c': ['rib', 'fib'],
    'test_fib6_perf.c': ['fib'],
    'test_fib_perf.c': ['net', 'fib'],
    'test_flow_classify.c': ['net', 'acl', 'table', 'ethdev', 'flow_classify'],
    'test_func_reentrancy.c': ['hash', 'lpm'],
    'test_graph.c': ['graph'],
    'test_graph_perf.c': ['graph'],
    'test_hash.c': ['net', 'hash'],
    'test_hash_functions.c': ['hash'],
    'test_hash_multiwriter.c': ['hash'],
    'test_hash_perf.c': ['hash'],
    'test_hash_readwrite.c': ['hash'],
    'test_hash_readwrite_lf_perf.c': ['hash'],
    'test_interrupts.c': [],
    'test_ipfrag.c': ['net', 'ip_frag'],
    'test_ipsec.c': ['bus_vdev', 'net', 'cryptodev', 'ipsec', 'security'],
    'test_ipsec_perf.c': ['net', 'ipsec'],
    'test_ipsec_sad.c': ['ipsec'],
    'test_kvargs.c': ['kvargs'],
    'test_latencystats.c': ['ethdev', 'latencystats', 'metrics'] + sample_packet_forward_deps,
    'test_lcores.c': [],
    'test_link_bonding.c': ['ethdev', 'net_bond',
        'net'] + packet_burst_generator_deps + virtual_pmd_deps,
    'test_link_bonding_mode4.c': ['ethdev', 'net_ring', 'net_bond',
        'net'] + packet_burst_generator_deps,
    'test_link_bonding_rssconf.c': ['ethdev', 'bus_vdev', 'net_bond'],
    'test_logs.c': [],
    'test_lpm.c': ['net', 'lpm'],
    'test_lpm6.c': ['lpm'],
    'test_lpm6_perf.c': ['lpm'],
    'test_lpm_perf.c': ['net', 'lpm'],
    'test_malloc.c': [],
    'test_malloc_perf.c': [],
    'test_mbuf.c': ['net'],
    'test_mcslock.c': [],
    'test_member.c': ['member', 'net'],
    'test_member_perf.c': ['hash', 'member'],
    'test_memcpy.c': [],
    'test_memcpy_perf.c': [],
    'test_memory.c': [],
    'test_mempool.c': [],
    'test_mempool_perf.c': [],
    'test_memzone.c': [],
    'test_meter.c': ['meter'],
    'test_metrics.c': ['metrics'],
    'test_mp_secondary.c': ['hash', 'lpm'],
    'test_net_ether.c': ['net'],
    'test_pcapng.c': ['ethdev', 'net', 'pcapng', 'bus_vdev'],
    'test_pdcp.c': ['eventdev', 'pdcp', 'net', 'timer', 'security'],
    'test_pdump.c': ['pdump'] + sample_packet_forward_deps,
    'test_per_lcore.c': [],
    'test_pflock.c': [],
    'test_pie.c': ['sched'],
    'test_pmd_perf.c': ['ethdev', 'net'] + packet_burst_generator_deps,
    'test_pmd_ring.c': ['net_ring', 'ethdev', 'bus_vdev'],
    'test_pmd_ring_perf.c': ['ethdev', 'net_ring', 'bus_vdev'],
    'test_power.c': ['power'],
    'test_power_cpufreq.c': ['power'],
    'test_power_intel_uncore.c': ['power'],
    'test_power_kvm_vm.c': ['power'],
    'test_prefetch.c': [],
    'test_rand_perf.c': [],
    'test_rawdev.c': ['rawdev', 'bus_vdev'],
    'test_rcu_qsbr.c': ['rcu', 'hash'],
    'test_rcu_qsbr_perf.c': ['rcu', 'hash'],
    'test_reassembly_perf.c': ['net', 'ip_frag'],
    'test_reciprocal_division.c': [],
    'test_reciprocal_division_perf.c': [],
    'test_red.c': ['sched'],
    'test_reorder.c': ['reorder'],
#    'test_resource.c': [],
    'test_rib.c': ['net', 'rib'],
    'test_rib6.c': ['net', 'rib'],
    'test_ring.c': [],
    'test_ring_hts_stress.c': [],
    'test_ring_mpmc_stress.c': [],
    'test_ring_mt_peek_stress.c': [],
    'test_ring_mt_peek_stress_zc.c': [],
    'test_ring_perf.c': [],
    'test_ring_rts_stress.c': [],
    'test_ring_st_peek_stress.c': [],
    'test_ring_st_peek_stress_zc.c': [],
    'test_ring_stress.c': [],
    'test_rwlock.c': [],
    'test_sched.c': ['net', 'sched'],
    'test_security.c': ['net', 'security'],
    'test_security_inline_macsec.c': ['ethdev', 'security'],
    'test_security_inline_proto.c': ['ethdev', 'security', 'eventdev'] + test_cryptodev_deps,
    'test_seqlock.c': [],
    'test_service_cores.c': [],
    'test_spinlock.c': [],
    'test_stack.c': ['stack'],
    'test_stack_perf.c': ['stack'],
    'test_string_fns.c': [],
    'test_table.c': ['table', 'pipeline', 'port'],
    'test_table_acl.c': ['net', 'table', 'pipeline', 'port'],
    'test_table_combined.c': ['table', 'pipeline', 'port'],
    'test_table_pipeline.c': ['pipeline', 'table', 'port'],
    'test_table_ports.c': ['table', 'pipeline', 'port'],
    'test_table_tables.c': ['table', 'pipeline', 'port'],
    'test_tailq.c': [],
    'test_telemetry_data.c': ['telemetry'],
    'test_telemetry_json.c': ['telemetry'],
    'test_thash.c': ['net', 'hash'],
    'test_thash_perf.c': ['hash'],
    'test_threads.c': [],
    'test_ticketlock.c': [],
    'test_timer.c': ['timer'],
    'test_timer_perf.c': ['timer'],
    'test_timer_racecond.c': ['timer'],
    'test_timer_secondary.c': ['timer'],
    'test_trace.c': [],
    'test_trace_perf.c': [],
    'test_trace_register.c': [],
    'test_vdev.c': ['kvargs', 'bus_vdev'],
    'test_version.c': [],
}

source_file_ext_deps = {
    'test_compressdev.c': ['zlib'],
    'test_pcapng.c': ['pcap'],
}

def_lib = get_option('default_library')
foreach f, f_deps : source_file_deps
    has_deps = true
    foreach d : f_deps
        if not is_variable(def_lib + '_rte_' + d)
            has_deps = false
            break
        else
            # technically we might not need this dep, but adding it is harmless
            if d not in deps
                deps += d
            endif
        endif
    endforeach
    # check for any external dependencies for this file
    if source_file_ext_deps.has_key(f)
        foreach d: source_file_ext_deps.get(f)
            dep = dependency(d, required: false, method: 'pkg-config')
            if not dep.found()
                message('Skipping test file @0@ due to missing external dependency @1@'.format(f, d))
                has_deps = false
            else
                ext_deps += dep
            endif
        endforeach
    endif
    if has_deps
        sources += files(f)
    endif
endforeach
# add the optional dependencies
foreach d:optional_deps
    if is_variable(def_lib + '_rte_' + d) and d not in deps
        deps += d
    endif
endforeach

if cc.has_argument('-Wno-format-truncation')
    cflags += '-Wno-format-truncation'
endif

# Strict-aliasing rules are violated by uint8_t[] to context size casts.
cflags += '-fno-strict-aliasing'

# Enable using internal APIs in unit tests
cflags += '-DALLOW_INTERNAL_API'

# create a symlink in the app/test directory for the binary, for backward compatibility
if not is_windows
    custom_target('test_symlink',
            output: 'dpdk-test',
            command: ['ln', '-sf', '../dpdk-test', '@OUTPUT@'],
            build_by_default: true,
            install: false)
endif
