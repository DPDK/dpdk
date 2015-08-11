API {#index}
===

<!--
  BSD LICENSE

  Copyright 2013 6WIND S.A.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of 6WIND S.A. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->

There are many libraries, so their headers may be grouped by topics:

- **device**:
  [dev]                (@ref rte_dev.h),
  [ethdev]             (@ref rte_ethdev.h),
  [ethctrl]            (@ref rte_eth_ctrl.h),
  [devargs]            (@ref rte_devargs.h),
  [bond]               (@ref rte_eth_bond.h),
  [vhost]              (@ref rte_virtio_net.h),
  [KNI]                (@ref rte_kni.h),
  [PCI]                (@ref rte_pci.h),
  [PCI IDs]            (@ref rte_pci_dev_ids.h)

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
  [rwlock]             (@ref rte_rwlock.h),
  [spinlock]           (@ref rte_spinlock.h)

- **CPU arch**:
  [branch prediction]  (@ref rte_branch_prediction.h),
  [cache prefetch]     (@ref rte_prefetch.h),
  [byte order]         (@ref rte_byteorder.h),
  [CPU flags]          (@ref rte_cpuflags.h)

- **CPU multicore**:
  [interrupts]         (@ref rte_interrupts.h),
  [launch]             (@ref rte_launch.h),
  [lcore]              (@ref rte_lcore.h),
  [per-lcore]          (@ref rte_per_lcore.h),
  [power/freq]         (@ref rte_power.h)

- **layers**:
  [ethernet]           (@ref rte_ether.h),
  [ARP]                (@ref rte_arp.h),
  [ICMP]               (@ref rte_icmp.h),
  [IP]                 (@ref rte_ip.h),
  [SCTP]               (@ref rte_sctp.h),
  [TCP]                (@ref rte_tcp.h),
  [UDP]                (@ref rte_udp.h),
  [frag/reass]         (@ref rte_ip_frag.h),
  [LPM IPv4 route]     (@ref rte_lpm.h),
  [LPM IPv6 route]     (@ref rte_lpm6.h),
  [ACL]                (@ref rte_acl.h)

- **QoS**:
  [metering]           (@ref rte_meter.h),
  [scheduler]          (@ref rte_sched.h),
  [RED congestion]     (@ref rte_red.h)

- **hashes**:
  [hash]               (@ref rte_hash.h),
  [jhash]              (@ref rte_jhash.h),
  [thash]              (@ref rte_thash.h),
  [FBK hash]           (@ref rte_fbk_hash.h),
  [CRC hash]           (@ref rte_hash_crc.h)

- **containers**:
  [mbuf]               (@ref rte_mbuf.h),
  [ring]               (@ref rte_ring.h),
  [distributor]        (@ref rte_distributor.h),
  [reorder]            (@ref rte_reorder.h),
  [tailq]              (@ref rte_tailq.h),
  [bitmap]             (@ref rte_bitmap.h),
  [ivshmem]            (@ref rte_ivshmem.h)

- **packet framework**:
  * [port]             (@ref rte_port.h):
    [ethdev]           (@ref rte_port_ethdev.h),
    [ring]             (@ref rte_port_ring.h),
    [frag]             (@ref rte_port_frag.h),
    [reass]            (@ref rte_port_ras.h),
    [sched]            (@ref rte_port_sched.h),
    [src/sink]         (@ref rte_port_source_sink.h)
  * [table]            (@ref rte_table.h):
    [lpm IPv4]         (@ref rte_table_lpm.h),
    [lpm IPv6]         (@ref rte_table_lpm_ipv6.h),
    [ACL]              (@ref rte_table_acl.h),
    [hash]             (@ref rte_table_hash.h),
    [array]            (@ref rte_table_array.h),
    [stub]             (@ref rte_table_stub.h)
  * [pipeline]         (@ref rte_pipeline.h)

- **basic**:
  [approx fraction]    (@ref rte_approx.h),
  [random]             (@ref rte_random.h),
  [config file]        (@ref rte_cfgfile.h),
  [key/value args]     (@ref rte_kvargs.h),
  [string]             (@ref rte_string_fns.h)

- **debug**:
  [jobstats]           (@ref rte_jobstats.h),
  [hexdump]            (@ref rte_hexdump.h),
  [debug]              (@ref rte_debug.h),
  [log]                (@ref rte_log.h),
  [warnings]           (@ref rte_warnings.h),
  [errno]              (@ref rte_errno.h)

- **misc**:
  [EAL config]         (@ref rte_eal.h),
  [common]             (@ref rte_common.h),
  [ABI compat]         (@ref rte_compat.h),
  [version]            (@ref rte_version.h)
