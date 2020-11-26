API
===

<!--
  SPDX-License-Identifier: BSD-3-Clause
  Copyright(c) 2013-2017 6WIND S.A.
-->

The public API headers are grouped by topics:

- **device**:
  [dev]                (@ref rte_dev.h),
  [ethdev]             (@ref rte_ethdev.h),
  [ethctrl]            (@ref rte_eth_ctrl.h),
  [rte_flow]           (@ref rte_flow.h),
  [rte_tm]             (@ref rte_tm.h),
  [rte_mtr]            (@ref rte_mtr.h),
  [bbdev]              (@ref rte_bbdev.h),
  [cryptodev]          (@ref rte_cryptodev.h),
  [security]           (@ref rte_security.h),
  [compressdev]        (@ref rte_compressdev.h),
  [compress]           (@ref rte_comp.h),
  [regexdev]           (@ref rte_regexdev.h),
  [eventdev]           (@ref rte_eventdev.h),
  [event_eth_rx_adapter]   (@ref rte_event_eth_rx_adapter.h),
  [event_eth_tx_adapter]   (@ref rte_event_eth_tx_adapter.h),
  [event_timer_adapter]    (@ref rte_event_timer_adapter.h),
  [event_crypto_adapter]   (@ref rte_event_crypto_adapter.h),
  [rawdev]             (@ref rte_rawdev.h),
  [metrics]            (@ref rte_metrics.h),
  [bitrate]            (@ref rte_bitrate.h),
  [latency]            (@ref rte_latencystats.h),
  [devargs]            (@ref rte_devargs.h),
  [PCI]                (@ref rte_pci.h),
  [vdev]               (@ref rte_bus_vdev.h),
  [vfio]               (@ref rte_vfio.h)

- **device specific**:
  [softnic]            (@ref rte_eth_softnic.h),
  [bond]               (@ref rte_eth_bond.h),
  [vhost]              (@ref rte_vhost.h),
  [vdpa]               (@ref rte_vdpa.h),
  [KNI]                (@ref rte_kni.h),
  [ark]                (@ref rte_pmd_ark.h),
  [ixgbe]              (@ref rte_pmd_ixgbe.h),
  [i40e]               (@ref rte_pmd_i40e.h),
  [ice]                (@ref rte_pmd_ice.h),
  [iavf]               (@ref rte_pmd_iavf.h),
  [ioat]               (@ref rte_ioat_rawdev.h),
  [bnxt]               (@ref rte_pmd_bnxt.h),
  [dpaa]               (@ref rte_pmd_dpaa.h),
  [dpaa2]              (@ref rte_pmd_dpaa2.h),
  [mlx5]               (@ref rte_pmd_mlx5.h),
  [dpaa2_mempool]      (@ref rte_dpaa2_mempool.h),
  [dpaa2_cmdif]        (@ref rte_pmd_dpaa2_cmdif.h),
  [dpaa2_qdma]         (@ref rte_pmd_dpaa2_qdma.h),
  [crypto_scheduler]   (@ref rte_cryptodev_scheduler.h),
  [dlb]                (@ref rte_pmd_dlb.h),
  [dlb2]               (@ref rte_pmd_dlb2.h)

- **memory**:
  [memseg]             (@ref rte_memory.h),
  [memzone]            (@ref rte_memzone.h),
  [mempool]            (@ref rte_mempool.h),
  [malloc]             (@ref rte_malloc.h),
  [memcpy]             (@ref rte_memcpy.h)

- **timers**:
  [cycles]             (@ref rte_cycles.h),
  [timer]              (@ref rte_timer.h),
  [alarm]              (@ref rte_alarm.h)

- **locks**:
  [atomic]             (@ref rte_atomic.h),
  [mcslock]            (@ref rte_mcslock.h),
  [rwlock]             (@ref rte_rwlock.h),
  [spinlock]           (@ref rte_spinlock.h),
  [ticketlock]         (@ref rte_ticketlock.h),
  [RCU]                (@ref rte_rcu_qsbr.h)

- **CPU arch**:
  [branch prediction]  (@ref rte_branch_prediction.h),
  [cache prefetch]     (@ref rte_prefetch.h),
  [SIMD]               (@ref rte_vect.h),
  [byte order]         (@ref rte_byteorder.h),
  [CPU flags]          (@ref rte_cpuflags.h),
  [CPU pause]          (@ref rte_pause.h),
  [I/O access]         (@ref rte_io.h)

- **CPU multicore**:
  [interrupts]         (@ref rte_interrupts.h),
  [launch]             (@ref rte_launch.h),
  [lcore]              (@ref rte_lcore.h),
  [per-lcore]          (@ref rte_per_lcore.h),
  [service cores]      (@ref rte_service.h),
  [keepalive]          (@ref rte_keepalive.h),
  [power/freq]         (@ref rte_power.h)

- **layers**:
  [ethernet]           (@ref rte_ether.h),
  [ARP]                (@ref rte_arp.h),
  [HIGIG]              (@ref rte_higig.h),
  [ICMP]               (@ref rte_icmp.h),
  [ESP]                (@ref rte_esp.h),
  [IPsec]              (@ref rte_ipsec.h),
  [IPsec group]        (@ref rte_ipsec_group.h),
  [IPsec SA]           (@ref rte_ipsec_sa.h),
  [IPsec SAD]          (@ref rte_ipsec_sad.h),
  [IP]                 (@ref rte_ip.h),
  [frag/reass]         (@ref rte_ip_frag.h),
  [SCTP]               (@ref rte_sctp.h),
  [TCP]                (@ref rte_tcp.h),
  [UDP]                (@ref rte_udp.h),
  [GTP]                (@ref rte_gtp.h),
  [GRO]                (@ref rte_gro.h),
  [GSO]                (@ref rte_gso.h),
  [GRE]                (@ref rte_gre.h),
  [MPLS]               (@ref rte_mpls.h),
  [VXLAN]              (@ref rte_vxlan.h),
  [Geneve]             (@ref rte_geneve.h),
  [eCPRI]              (@ref rte_ecpri.h)

- **QoS**:
  [metering]           (@ref rte_meter.h),
  [scheduler]          (@ref rte_sched.h),
  [RED congestion]     (@ref rte_red.h)

- **routing**:
  [LPM IPv4 route]     (@ref rte_lpm.h),
  [LPM IPv6 route]     (@ref rte_lpm6.h),
  [RIB IPv4]           (@ref rte_rib.h),
  [RIB IPv6]           (@ref rte_rib6.h),
  [FIB IPv4]           (@ref rte_fib.h),
  [FIB IPv6]           (@ref rte_fib6.h)

- **hashes**:
  [hash]               (@ref rte_hash.h),
  [jhash]              (@ref rte_jhash.h),
  [thash]              (@ref rte_thash.h),
  [FBK hash]           (@ref rte_fbk_hash.h),
  [CRC hash]           (@ref rte_hash_crc.h)

- **classification**
  [reorder]            (@ref rte_reorder.h),
  [distributor]        (@ref rte_distributor.h),
  [EFD]                (@ref rte_efd.h),
  [ACL]                (@ref rte_acl.h),
  [member]             (@ref rte_member.h),
  [flow classify]      (@ref rte_flow_classify.h),
  [BPF]                (@ref rte_bpf.h)

- **containers**:
  [mbuf]               (@ref rte_mbuf.h),
  [mbuf pool ops]      (@ref rte_mbuf_pool_ops.h),
  [ring]               (@ref rte_ring.h),
  [stack]              (@ref rte_stack.h),
  [tailq]              (@ref rte_tailq.h),
  [bitmap]             (@ref rte_bitmap.h)

- **packet framework**:
  * [port]             (@ref rte_port.h):
    [ethdev]           (@ref rte_port_ethdev.h),
    [ring]             (@ref rte_port_ring.h),
    [frag]             (@ref rte_port_frag.h),
    [reass]            (@ref rte_port_ras.h),
    [sched]            (@ref rte_port_sched.h),
    [kni]              (@ref rte_port_kni.h),
    [src/sink]         (@ref rte_port_source_sink.h)
  * [table]            (@ref rte_table.h):
    [lpm IPv4]         (@ref rte_table_lpm.h),
    [lpm IPv6]         (@ref rte_table_lpm_ipv6.h),
    [ACL]              (@ref rte_table_acl.h),
    [hash]             (@ref rte_table_hash.h),
    [array]            (@ref rte_table_array.h),
    [stub]             (@ref rte_table_stub.h)
  * [pipeline]         (@ref rte_pipeline.h)
    [port_in_action]   (@ref rte_port_in_action.h)
    [table_action]     (@ref rte_table_action.h)
  * SWX pipeline:
    [control]          (@ref rte_swx_ctl.h),
    [extern]           (@ref rte_swx_extern.h),
    [pipeline]         (@ref rte_swx_pipeline.h)
  * SWX port:
    [port]             (@ref rte_swx_port.h),
    [ethdev]           (@ref rte_swx_port_ethdev.h),
    [src/sink]         (@ref rte_swx_port_source_sink.h)
  * SWX table:
    [table]            (@ref rte_swx_table.h),
    [table_em]         (@ref rte_swx_table_em.h)
  * [graph]            (@ref rte_graph.h):
    [graph_worker]     (@ref rte_graph_worker.h)
  * graph_nodes:
    [eth_node]         (@ref rte_node_eth_api.h),
    [ip4_node]         (@ref rte_node_ip4_api.h)

- **basic**:
  [bitops]             (@ref rte_bitops.h),
  [approx fraction]    (@ref rte_approx.h),
  [random]             (@ref rte_random.h),
  [config file]        (@ref rte_cfgfile.h),
  [key/value args]     (@ref rte_kvargs.h),
  [string]             (@ref rte_string_fns.h)

- **debug**:
  [jobstats]           (@ref rte_jobstats.h),
  [telemetry]          (@ref rte_telemetry.h),
  [pdump]              (@ref rte_pdump.h),
  [hexdump]            (@ref rte_hexdump.h),
  [debug]              (@ref rte_debug.h),
  [log]                (@ref rte_log.h),
  [errno]              (@ref rte_errno.h),
  [trace]              (@ref rte_trace.h),
  [trace_point]        (@ref rte_trace_point.h)

- **misc**:
  [EAL config]         (@ref rte_eal.h),
  [common]             (@ref rte_common.h),
  [experimental APIs]  (@ref rte_compat.h),
  [ABI versioning]     (@ref rte_function_versioning.h),
  [version]            (@ref rte_version.h)
