.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 24.11
==================

New Features
------------

* **Added new bit manipulation API.**

  Extended support for bit-level operations on single 32 and 64-bit words in
  ``<rte_bitops.h>`` with semantically well-defined functions.

  * ``rte_bit_[test|set|clear|assign|flip]`` functions provide excellent
    performance (by avoiding restricting the compiler and CPU), but give
    no guarantees in relation to memory ordering or atomicity.

  * ``rte_bit_atomic_*`` provide atomic bit-level operations including
    the possibility to specify memory ordering constraints.

  The new public API elements are polymorphic, using the _Generic-based
  macros (for C) and function overloading (in C++ translation units).

* **Added multi-word bitset API.**

  Introduced a new multi-word bitset API to the EAL.

  The RTE bitset is optimized for scenarios where the bitset size exceeds the
  capacity of a single word (e.g., larger than 64 bits), but is not large
  enough to justify the overhead and complexity of the more scalable,
  yet slower, ``<rte_bitmap.h>`` API.

  This addition provides an efficient and straightforward alternative
  for handling bitsets of intermediate size.

* **Added a per-lcore static memory allocation facility.**

  Added EAL API ``<rte_lcore_var.h>`` for statically allocating small,
  frequently-accessed data structures, for which one instance should exist
  for each EAL thread and registered non-EAL thread.

  With lcore variables, data is organized spatially on a per-lcore id basis,
  rather than per library or PMD, avoiding the need for cache aligning
  (or RTE_CACHE_GUARDing) data structures, which in turn
  reduces CPU cache internal fragmentation and improves performance.

  Lcore variables are similar to thread-local storage (TLS, e.g. C11 ``_Thread_local``),
  but decouples the values' life times from those of the threads.

* **Extended service cores statistics.**

  Two new per-service counters are added to the service cores framework.

  * ``RTE_SERVICE_ATTR_IDLE_CALL_COUNT`` tracks the number of service function
    invocations where no actual work was performed.

  * ``RTE_SERVICE_ATTR_ERROR_CALL_COUNT`` tracks the number of invocations
    resulting in an error.

  The new statistics are useful for debugging and profiling.

* **Hardened rte_malloc and related functions.**

  Added function attributes to ``rte_malloc`` and similar functions
  that can catch some obvious bugs at compile time (with GCC 11.0 or later).
  For example, calling ``free`` on a pointer that was allocated with ``rte_malloc``
  (and vice versa); freeing the same pointer twice in the same routine or
  freeing an object that was not created by allocation.

* **Updated logging library.**

  * The log subsystem is initialized earlier in startup so all messages go through the library.

  * If the application is a systemd service and the log output is being sent to standard error
    then DPDK will switch to journal native protocol.
    This allows more data such as severity to be sent.

  * The syslog option has changed.
    By default, messages are no longer sent to syslog unless the ``--syslog`` option is specified.
    Syslog is also supported on FreeBSD (but not on Windows).

  * Log messages can be timestamped with ``--log-timestamp`` option.

  * Log messages can be colorized with the ``--log-color`` option.

* **Updated Marvell cnxk mempool driver.**

  * Added mempool driver support for CN20K SoC.

* **Added more ICMP message types and codes.**

  Added new ICMP message types and codes from RFC 792 in ``rte_icmp.h``.

* **Added IPv6 address structure and related utilities.**

  A new IPv6 address structure is now available in ``rte_ip6.h``.
  It comes with a set of helper functions and macros.

* **Added link speed lanes API.**

  Added functions to query or force the link lanes configuration.

* **Added Ethernet device clock frequency adjustment.**

  Added the function ``rte_eth_timesync_adjust_freq``
  to adjust the clock frequency for Ethernet devices.

* **Extended flow table index features.**

  * Extended the flow table insertion type enum with the
    ``RTE_FLOW_TABLE_INSERTION_TYPE_INDEX_WITH_PATTERN`` type.
  * Added a function for inserting a flow rule by index with pattern:
    ``rte_flow_async_create_by_index_with_pattern()``.
  * Added a flow action to redirect packets to a particular index in a flow table:
    ``RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX``.

* **Added support for dumping registers with names and filtering by modules.**

  Added a new function ``rte_eth_dev_get_reg_info_ext()``
  to filter the registers by module names and get the information
  (names, values and other attributes) of the filtered registers.

* **Updated Amazon ENA (Elastic Network Adapter) net driver.**

  * Modified the PMD API that controls the LLQ header policy.
  * Replaced ``enable_llq``, ``normal_llq_hdr`` and ``large_llq_hdr`` devargs
    with a new shared devarg ``llq_policy`` that maintains the same logic.
  * Added a validation check for Rx packet descriptor consistency.

* **Updated Cisco enic driver.**

  * Added SR-IOV VF support.
  * Added recent 1400/14000 and 15000 models to the supported list.

* **Updated Marvell cnxk net driver.**

  * Added ethdev driver support for CN20K SoC.

* **Updated Napatech ntnic net driver [EXPERIMENTAL].**

  * Updated supported version of the FPGA to 9563.55.49.
  * Extended and fixed logging.
  * Added:

    - NT flow filter initialization.
    - NT flow backend initialization.
    - Initialization of FPGA modules related to flow HW offload.
    - Basic handling of the virtual queues.
    - Flow handling support.
    - Statistics support.
    - Age flow action support.
    - Meter flow metering and flow policy support.
    - Flow actions update support.
    - Asynchronous flow support.
    - MTU update support.

* **Updated NVIDIA mlx5 net driver.**

  * Added ``rte_flow_async_create_by_index_with_pattern()`` support.
  * Added jump to flow table index support.

* **Added Realtek r8169 net driver.**

  Added a new network PMD which supports Realtek 2.5 and 5 Gigabit
  Ethernet NICs.

* **Added ZTE zxdh net driver [EXPERIMENTAL].**

  Added ethdev driver support for the zxdh NX Series Ethernet Controller.
  This has:

  * The ability to initialize the NIC.
  * No datapath support.

* **Added cryptodev queue pair reset support.**

  A new API ``rte_cryptodev_queue_pair_reset`` is added
  to reset a particular queue pair of a device.

* **Added cryptodev asymmetric EdDSA support.**

  Added asymmetric EdDSA as referenced in `RFC 8032
  <https://datatracker.ietf.org/doc/html/rfc8032>`_.

* **Added cryptodev SM4-XTS support.**

  Added symmetric cipher algorithm ShangMi 4 (SM4) in XTS mode.

* **Updated IPsec_MB crypto driver.**

  * Added support for the SM3 algorithm.
  * Added support for the SM3 HMAC algorithm.
  * Added support for the SM4 CBC, SM4 ECB and SM4 CTR algorithms.
  * Bumped the minimum version requirement of Intel IPsec Multi-buffer library to v1.4.
    Affected PMDs: KASUMI, SNOW3G, ZUC, AESNI GCM, AESNI MB and CHACHAPOLY.

* **Updated openssl crypto driver.**

  * Added support for asymmetric crypto EdDSA algorithm.

* **Updated Marvell cnxk crypto driver.**

  * Added support for asymmetric crypto EdDSA algorithm.

* **Added stateless IPsec processing.**

  New functions were added to enable
  providing sequence number to be used for the IPsec operation.

* **Added strict priority capability for dmadev.**

  Added new capability flag ``RTE_DMA_CAPA_PRI_POLICY_SP``
  to check if the DMA device supports assigning fixed priority,
  allowing for better control over resource allocation and scheduling.

* **Updated Marvell cnxk DMA driver.**

  * Added support for DMA queue priority configuration.

* **Added Marvell cnxk RVU LF rawdev driver.**

  Added a new raw device driver for Marvell cnxk based devices
  to allow an out-of-tree driver to manage a RVU LF device.
  It enables operations such as sending/receiving mailbox,
  register and notify the interrupts, etc.

* **Added event device pre-scheduling support.**

  Added support for pre-scheduling of events to event ports
  to improve scheduling performance and latency.

  * Added ``rte_event_dev_config::preschedule_type``
    to configure the device level pre-scheduling type.

  * Added ``rte_event_port_preschedule_modify``
    to modify pre-scheduling type on a given event port.

  * Added ``rte_event_port_preschedule``
    to allow applications provide explicit pre-schedule hints to event ports.

* **Updated event device library for independent enqueue feature.**

  Added support for independent enqueue feature.
  With this feature eventdev supports enqueue in any order
  or specifically in a different order than dequeue.
  The feature is intended for eventdevs supporting burst mode.
  Applications should use ``RTE_EVENT_PORT_CFG_INDEPENDENT_ENQ`` to enable
  the feature if the capability ``RTE_EVENT_DEV_CAP_INDEPENDENT_ENQ`` exists.

* **Updated DLB2 event driver.**

  * Added independent enqueue feature.

* **Updated DSW event driver.**

  * Added independent enqueue feature.

* **Updated Marvell cnxk event device driver.**

  * Added eventdev driver support for CN20K SoC.

* **Added IPv4 network order lookup in the FIB library.**

  A new flag field is introduced in the ``rte_fib_conf`` structure.
  This field is used to pass an extra configuration settings such as the ability
  to lookup IPv4 addresses in network byte order.

* **Added RSS hash key generating API.**

  A new function ``rte_thash_gen_key`` is provided to modify the RSS hash key
  to achieve better traffic distribution with RSS.

* **Added per-CPU power management QoS interface.**

  Added per-CPU PM QoS interface to lower the resume latency
  when waking up from idle state.

* **Added new API to register telemetry endpoint callbacks with private arguments.**

  A new ``rte_telemetry_register_cmd_arg`` function is available to pass an opaque value to
  telemetry endpoint callback.

* **Added node specific statistics.**

  Added ability for a node to advertise and update multiple xstat counters,
  that can be retrieved using ``rte_graph_cluster_stats_get``.


Removed Items
-------------

* ethdev: Removed the ``__rte_ethdev_trace_rx_burst`` symbol, as the corresponding
  tracepoint was split into two separate ones for empty and non-empty calls.


API Changes
-----------

* kvargs: reworked the process API.

  * The already existing ``rte_kvargs_process`` now only handles ``key=value`` cases and
    rejects input where only a key is present in the parsed string.
  * ``rte_kvargs_process_opt`` has been added to behave as ``rte_kvargs_process`` in previous
    releases: it handles key=value and only-key cases.
  * Both ``rte_kvargs_process`` and ``rte_kvargs_process_opt`` reject a NULL ``kvlist`` parameter.

* net: The IPv4 header structure ``rte_ipv4_hdr`` has been marked as two bytes aligned.

* net: The ICMP message types ``RTE_IP_ICMP_ECHO_REPLY`` and ``RTE_IP_ICMP_ECHO_REQUEST``
  are marked as deprecated, and are replaced
  by ``RTE_ICMP_TYPE_ECHO_REPLY`` and ``RTE_ICMP_TYPE_ECHO_REQUEST``.

* net: The IPv6 header structure ``rte_ipv6_hdr`` and extension structures ``rte_ipv6_routing_ext``
  and ``rte_ipv6_fragment_ext`` have been marked as two bytes aligned.

* net: A new IPv6 address structure was introduced to replace ad-hoc ``uint8_t[16]`` arrays.
  The following libraries and symbols were modified:

  - cmdline:

    - ``cmdline_ipaddr_t``

  - ethdev:

    - ``struct rte_flow_action_set_ipv6``
    - ``struct rte_flow_item_icmp6_nd_na``
    - ``struct rte_flow_item_icmp6_nd_ns``
    - ``struct rte_flow_tunnel``

  - fib:

    - ``rte_fib6_add()``
    - ``rte_fib6_delete()``
    - ``rte_fib6_lookup_bulk()``
    - ``RTE_FIB6_IPV6_ADDR_SIZE`` (deprecated, replaced with ``RTE_IPV6_ADDR_SIZE``)
    - ``RTE_FIB6_MAXDEPTH`` (deprecated, replaced with ``RTE_IPV6_MAX_DEPTH``)

  - hash:

    - ``struct rte_ipv6_tuple``

  - ipsec:

    - ``struct rte_ipsec_sadv6_key``

  - lpm:

    - ``rte_lpm6_add()``
    - ``rte_lpm6_delete()``
    - ``rte_lpm6_delete_bulk_func()``
    - ``rte_lpm6_is_rule_present()``
    - ``rte_lpm6_lookup()``
    - ``rte_lpm6_lookup_bulk_func()``
    - ``RTE_LPM6_IPV6_ADDR_SIZE`` (deprecated, replaced with ``RTE_IPV6_ADDR_SIZE``)
    - ``RTE_LPM6_MAX_DEPTH`` (deprecated, replaced with ``RTE_IPV6_MAX_DEPTH``)

  - net:

    - ``struct rte_ipv6_hdr``

  - node:

    - ``rte_node_ip6_route_add()``

  - pipeline:

    - ``struct rte_swx_ipsec_sa_encap_params``
    - ``struct rte_table_action_ipv6_header``
    - ``struct rte_table_action_nat_params``

  - security:

    - ``struct rte_security_ipsec_tunnel_param``

  - table:

    - ``struct rte_table_lpm_ipv6_key``
    - ``RTE_LPM_IPV6_ADDR_SIZE`` (deprecated, replaced with ``RTE_IPV6_ADDR_SIZE``)

  - rib:

    - ``rte_rib6_get_ip()``
    - ``rte_rib6_get_nxt()``
    - ``rte_rib6_insert()``
    - ``rte_rib6_lookup()``
    - ``rte_rib6_lookup_exact()``
    - ``rte_rib6_remove()``
    - ``RTE_RIB6_IPV6_ADDR_SIZE`` (deprecated, replaced with ``RTE_IPV6_ADDR_SIZE``)
    - ``get_msk_part()`` (deprecated)
    - ``rte_rib6_copy_addr()`` (deprecated, replaced with direct structure assignments)
    - ``rte_rib6_is_equal()`` (deprecated, replaced with ``rte_ipv6_addr_eq()``)

* drivers/net/ena: Removed ``enable_llq``, ``normal_llq_hdr`` and ``large_llq_hdr`` devargs
  and replaced it with a new shared devarg ``llq_policy`` that keeps the same logic.


ABI Changes
-----------

* eal: The maximum number of file descriptors that can be passed to a secondary process
  has been increased from 8 to 253 (which is the maximum possible with Unix domain sockets).
  This allows for more queues when using software devices such as TAP and XDP.

* ethdev: Added ``filter`` and ``names`` fields to ``rte_dev_reg_info`` structure
  for filtering by modules and reporting names of registers.

* cryptodev: The queue pair configuration structure ``rte_cryptodev_qp_conf``
  is updated to have a new parameter to set priority of that particular queue pair.

* cryptodev: The list end enumerators ``RTE_CRYPTO_ASYM_XFORM_TYPE_LIST_END``
  and ``RTE_CRYPTO_RSA_PADDING_TYPE_LIST_END`` are removed
  to allow subsequent addition of new asymmetric algorithms and RSA padding types.

* cryptodev: The enum ``rte_crypto_asym_xform_type`` and struct ``rte_crypto_asym_op``
  are updated to include new values to support EdDSA.

* cryptodev: The ``rte_crypto_rsa_xform`` struct member to hold private key data
  in either exponent or quintuple format is changed from a union to a struct data type.
  This change is to support ASN.1 syntax (RFC 3447 Appendix A.1.2).

* cryptodev: The padding struct ``rte_crypto_rsa_padding`` is moved
  from ``rte_crypto_rsa_op_param`` to ``rte_crypto_rsa_xform``
  as the padding information is part of session creation
  instead of the per packet crypto operation.
  This change is required to support virtio-crypto specifications.

* bbdev: The structure ``rte_bbdev_stats`` was updated to add a new parameter
  to optionally report the number of enqueue batches available ``enqueue_depth_avail``.

* dmadev: Added ``nb_priorities`` field to the ``rte_dma_info`` structure
  and ``priority`` field to the ``rte_dma_conf`` structure
  to get device supported priority levels
  and configure required priority from the application.

* eventdev: Added the ``preschedule_type`` field to ``rte_event_dev_config`` structure.

* eventdev: Removed the single-event enqueue and dequeue function pointers
  from ``rte_event_fp_fps``.

* graph: To accommodate node specific xstats counters, added ``xstat_cntrs``,
  ``xstat_desc`` and ``xstat_count`` to ``rte_graph_cluster_node_stats``,
  added new structure ``rte_node_xstats`` to ``rte_node_register`` and
  added ``xstat_off`` to ``rte_node``.

* graph: The members ``dispatch`` and ``xstat_off`` of the structure ``rte_node``
  have been marked as ``RTE_CACHE_LINE_MIN_SIZE`` bytes aligned.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel Atom\ |reg| P5342 processor
    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| D-1747NTE CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| D-2796NT CPU @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8358 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8468H
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H

  * OS:

    * Microsoft Azure Linux 3.0
    * Fedora 40
    * FreeBSD 14.1
    * OpenAnolis OS 8.9
    * openEuler 24.03 (LTS)
    * Red Hat Enterprise Linux Server release 9.4
    * Ubuntu 22.04.3
    * Ubuntu 22.04.4
    * Ubuntu 24.04
    * Ubuntu 24.04.1

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.60 0x8001e8b2 1.3682.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-tree): 1.15.4 (ice)
      * Driver version(in-tree): 6.8.0-48-generic (Ubuntu24.04.1) /
        5.14.0-427.13.1.el9_4.x86_64+rt (RHEL9.4) (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.60 0x8001e8b1 1.3682.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version(out-tree): 1.15.4 (ice)
      * Driver version(in-tree): 6.6.12.1-1.azl3+ice+ (Microsoft Azure Linux 3.0) (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.60 0x8001e8b0 1.3682.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 1.15.4 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0

    * Intel\ |reg| Ethernet Connection E823-C for QSFP

      * Firmware version: 3.42 0x8001f66b 1.3682.0
      * Device id (pf/vf): 8086:188b / 8086:1889
      * Driver version: 1.15.4 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Connection E823-L for QSFP

      * Firmware version: 3.42 0x8001ea89 1.3636.0
      * Device id (pf/vf): 8086:124c / 8086:1889
      * Driver version: 1.15.4 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Connection E822-L for backplane

      * Firmware version: 3.42 0x8001eaad 1.3636.0
      * Device id (pf/vf): 8086:1897 / 8086:1889
      * Driver version: 1.15.4 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Connection E825-C for QSFP

      * Firmware version: 3.75 0x80005600 1.3643.0
      * Device id (pf/vf): 8086:579d / 8086:1889
      * Driver version: 2.1.0_rc23 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Network Adapter E830-XXVDA2 for OCP

      * Firmware version: 1.00 0x8000942a 1.3672.0
      * Device id (pf/vf): 8086:12d3 / 8086:1889
      * Driver version: 1.15.4 (ice)
      * OS Default DDP: 1.3.38.0

    * Intel\ |reg| Ethernet Network Adapter E830-CQDA2

      * Firmware version: 1.00 0x8000d294 1.3722.0
      * Device id (pf/vf): 8086:12d2 / 8086:1889
      * Driver version: 1.15.4 (ice)
      * OS Default DDP: 1.3.39.0
      * COMMS DDP: 1.3.51.0
      * Wireless Edge DDP: 1.3.19.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x000161bf
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 5.21.5 (ixgbe)
      * Driver version(in-tree): 6.8.0-48-generic (Ubuntu24.04.1)

    * Intel\ |reg| Ethernet Network Adapter E610-XT2

      * Firmware version: 1.00 0x800066ae 0.0.0
      * Device id (pf/vf): 8086:57b0 / 8086:57ad
      * Driver version(out-tree): 5.21.5 (ixgbe)

    * Intel\ |reg| Ethernet Network Adapter E610-XT4

      * Firmware version: 1.00 0x80004ef2 0.0.0
      * Device id (pf/vf): 8086:57b0 / 8086:57ad
      * Driver version(out-tree): 5.21.5 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.50 0x8000f4c6 1.3682.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.26.8 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.50 0x80004216 1.3597.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.26.8 (i40e)
      * Driver version(in-tree): 5.14.0-427.13.1.el9_4.x86_64 (RHEL9.4)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version:  9.50 0x8000f4e1 1.3682.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.26.8 (i40e)
      * Driver version(in-tree): 6.8.0-45-generic (Ubuntu24.04.1) /
        5.14.0-427.13.1.el9_4.x86_64 (RHEL9.4)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.50 0x8000f4fe 1.3682.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.26.8 (i40e)

    * Intel\ |reg| Ethernet Controller I225-LM

      * Firmware version: 1.3, 0x800000c9
      * Device id (pf): 8086:15f2
      * Driver version(in-tree): 6.8.0-48-generic (Ubuntu24.04.1)(igc)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device id (pf): 8086:125b
      * Driver version(in-tree): 6.8.0-45-generic (Ubuntu24.04.1)(igc)

    * Intel Corporation Ethernet Server Adapter I350-T4

      * Firmware version: 1.63, 0x80001001
      * Device id (pf): 8086:1521 /8086:1520
      * Driver version: 6.6.25-lts-240422t024020z(igb)

* Intel\ |reg| platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2670 0 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2640 @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 0 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2620 v4 @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux release 9.1 (Plow)
    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Red Hat Enterprise Linux release 8.4 (Ootpa)
    * Ubuntu 22.04
    * Ubuntu 20.04
    * SUSE Enterprise Linux 15 SP2

  * OFED:

    * MLNX_OFED 24.10-0.7.0.0 and above

  * DOCA:
    * doca 2.9.0-0.4.7 and above

  * upstream kernel:

    * Linux 6.12.0 and above

  * rdma-core:

    * rdma-core-54.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.43.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.43.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.43.1014 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.43.1014 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.43.1014 and above

  * Embedded software:

    * Ubuntu 22.04
    * MLNX_OFED 24.10-0.6.7.0 and above
    * bf-bundle-2.9.0-90_24.10_ubuntu-22.04
    * DPDK application running on ARM cores

* IBM Power 9 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Ubuntu 20.04

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.43.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.43.1014 and above

  * OFED:

    * MLNX_OFED 24.10-0.7.0.0

24.11.1 Release Notes
---------------------


24.11.1 Fixes
~~~~~~~~~~~~~


24.11.1 Validation
~~~~~~~~~~~~~~~~~~

* Tested by Red Hat validation team

24.11.1 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 24.11.1 contains DPDK 24.11 plus the fix for CVE-2024-11614 only

24.11.2 Release Notes
---------------------


24.11.2 Fixes
~~~~~~~~~~~~~

* app/testpmd: avoid crash in DCB config
* app/testpmd: fix out-of-bound reference in offload config
* app/testpmd: show all DCB priority TC map
* app/testpmd: use VXLAN flow flags from user if set
* baseband/acc: fix queue setup failure clean up
* build: force GCC 15 to initialize padding bits
* buildtools: fix some Python regex syntax warnings
* bus/pci: fix registered device name
* ci: build with MSVC in GHA
* ci: fix ccache for Ubuntu 22.04
* ci: point at GitHub mirror
* common/cnxk: fix atomic load in batch ops
* common/cnxk: fix DPI mailbox structure
* common/cnxk: fix inbound IPsec SA setup
* common/cnxk: fix null check
* common/idpf: fix void function returning a value
* common/mlx5: add device duplication function
* common/qat: fix devargs parsing
* crypto/cnxk: fix asymmetric operation status code
* crypto/cnxk: fix build with GCC 15
* cryptodev: fix C++ include
* crypto/dpaa2_sec: fix bitmask truncation
* crypto/dpaa_sec: fix bitmask truncation
* crypto/openssl: fix CMAC auth context update
* crypto/openssl: validate incorrect RSA signature
* crypto/qat: fix SM3 state size
* crypto/virtio: fix data queues iteration
* crypto/virtio: fix redundant queue free
* dma/idxd: add device ids for new HW versions
* doc: add no-IOMMU mode in devbind tool guide
* doc: add tested platforms with NVIDIA NICs in 24.11
* doc: add two more tested Intel NICs in 24.11
* doc: add VXLAN matching requirement in mlx5 guide
* doc: fix feature flags for queue start/stop
* doc: fix year of final LTS release
* doc: update ionic driver guide
* dts: fix MTU set command
* dts: fix pass rate edge case in results
* dts: fix smoke tests docstring
* eal: fix devargs layers parsing out of bounds
* eal: fix undetected NUMA nodes
* eal/linux: fix memseg length in legacy mem init
* eal/linux: remove useless assignments
* eal/x86: defer power intrinsics variable allocation
* eal/x86: fix some intrinsics header include for Windows
* ethdev: fix functions available in new device event
* ethdev: fix registers info request
* eventdev: fix format string data type in log messages
* event/dlb2: fix event weight handling in SSE code path
* event/dpaa: fix bitmask truncation
* examples/flow_filtering: fix build with GCC 15
* examples/flow_filtering: fix IPv4 matching snippet
* examples/flow_filtering: remove duplicate assignment
* examples/ipsec-secgw: fix cryptodev and eventdev IDs
* examples/ipsec-secgw: fix IV length in CTR 192/256
* examples/l3fwd: fix socket ID check
* examples/ptpclient: fix message parsing
* examples/vhost_crypto: fix user callbacks
* gro: fix unprocessed IPv4 packets
* log: fix double free on cleanup
* log: fix systemd journal socket open
* mempool: fix errno in empty create
* net/af_packet: fix socket close on device stop
* net/bnxt: address uninitialized variables
* net/bnxt: fix crash when representor is re-attached
* net/bnxt: fix dead code
* net/bnxt: fix epoch bit calculation
* net/bnxt: fix indication of allocation
* net/bnxt: fix overflow
* net/bnxt: fix overflow
* net/bnxt: fix Rx handler
* net/bonding: fix dedicated queue setup
* net/cnxk: fix NIX send header L3 type
* net/cpfl: fix representor parsing log
* net/dpaa2: fix bitmask truncation
* net/dpaa: fix bitmask truncation
* net/e1000/base: correct mPHY access logic
* net/e1000/base: fix bitwise operation type
* net/e1000/base: fix data type in MAC hash
* net/e1000/base: fix iterator type
* net/e1000/base: fix MAC address hash bit shift
* net/e1000/base: fix NVM data type in bit shift
* net/e1000/base: fix reset for 82580
* net/e1000/base: fix semaphore timeout value
* net/e1000/base: fix unchecked return
* net/e1000/base: fix uninitialized variable
* net/e1000/base: skip management check for 82575
* net/e1000: fix crashes in secondary processes
* net/ena: fix missing default LLQ policy
* net/enetfec: remove useless assignment
* net/gve: allocate Rx QPL pages using malloc
* net/hinic: fix flow type bitmask overflow
* net/hns3: fix copper port initialization
* net/hns3: fix mbuf freeing in simple Tx path
* net/hns3: fix reset timeout
* net/hns3: remove PVID info dump for VF
* net/hns3: rename RAS module
* net/iavf: check interrupt registration failure
* net/iavf: fix crash on app exit on FreeBSD
* net/iavf: fix mbuf release in Arm multi-process
* net/iavf: remove reset of Tx prepare function pointer
* net/ice: fix dropped packets when using VRRP
* net/ice: fix flow engines order
* net/ice: fix flows handling
* net/ice: fix memory leak in scalar Rx
* net/igc/base: fix bitwise operation type
* net/igc/base: fix data type in MAC hash
* net/igc/base: fix deadlock when writing i225 register
* net/igc/base: fix infinite loop
* net/igc/base: fix iterator type
* net/igc/base: fix LTR for i225
* net/igc/base: fix MAC address hash bit shift
* net/igc/base: fix NVM data type in bit shift
* net/igc/base: fix semaphore timeout value
* net/igc/base: fix typo in LTR calculation
* net/igc/base: fix unused value
* net/igc/base: increase PHY power up delay
* net/igc/base: reset loop variable
* net/intel: fix build with icx
* net/intel: fix void functions returning a value
* net/ixgbe: add checks for E610 VF
* net/ixgbe/base: add missing buffer copy for ACI
* net/ixgbe/base: fix driver hang in VM
* net/ixgbe/base: fix TSAM checking return value
* net/ixgbe/base: remove 2.5/5G from auto-negotiation for E610
* net/ixgbe: fix crashes in secondary processes
* net/ixgbe: fix E610 support in flow engine
* net/ixgbe: fix minimum Rx/Tx descriptors
* net/ixgbe: fix VF registers for E610
* net/mana: do not ring short doorbell for every mbuf alloc
* net/mana: fix multi-process tracking
* net/mlx5: adjust actions per rule limitation
* net/mlx5: fix actions translation error overwrite
* net/mlx5: fix crash in non-template metadata split
* net/mlx5: fix crash with null flow list creation
* net/mlx5: fix error info in actions construct
* net/mlx5: fix flow group ID for action translation
* net/mlx5: fix flow matching GENEVE options
* net/mlx5: fix flush of non-template flow rules
* net/mlx5: fix GENEVE parser cleanup
* net/mlx5: fix GRE flow match with SWS
* net/mlx5: fix GRE matching on root table
* net/mlx5: fix hairpin queue release
* net/mlx5: fix hardware packet type translation
* net/mlx5: fix IPIP tunnel verification
* net/mlx5: fix LACP packet handling in isolated mode
* net/mlx5: fix leak in HWS flow counter action
* net/mlx5: fix leak of flow action data list
* net/mlx5: fix mark action validation in FDB mode
* net/mlx5: fix NAT64 register selection
* net/mlx5: fix Netlink socket leak
* net/mlx5: fix non-template flow validation
* net/mlx5: fix non-template flow validation on create
* net/mlx5: fix non-template set VLAN VID
* net/mlx5: fix polling CQEs
* net/mlx5: fix unneeded stub flow table allocation
* net/mlx5/hws: fix crash using represented port without ID
* net/mlx5/hws: fix DV FT type convert
* net/mlx5/hws: fix fragmented packet type matching
* net/mlx5/hws: fix GTP flags matching
* net/netvsc: remove device if its net devices removed
* net/netvsc: scan all net devices under the PCI device
* net/nfp: fix firmware load from flash
* net/nfp: fix init failure handling
* net/nfp: fix misuse of function return values
* net/nfp: fix multi-PF control flag
* net/nfp: fix multiple PFs check from NSP
* net/nfp: fix representor port statistics
* net/nfp: fix VF link speed
* net/ngbe: fix WOL and NCSI capabilities
* net/octeon_ep: remove useless assignment
* net/qede: fix debug messages array
* net/qede: fix nested loops
* net/sfc: remove unnecessary assignment
* net/thunderx/base: fix build with GCC 15
* net/txgbe: remove useless condition for SW-FW sync
* pdump: clear statistics when enabled
* power: defer lcore variable allocation
* ptr_compress: fix build with Arm SVE
* random: defer seeding to EAL init
* raw/cnxk_gpio: fix file descriptor leak
* service: fix getting service lcore attributes
* stack: fix pop in C11 implementation
* test/bbdev: update FFT test vectors
* test/bonding: fix active backup receive test
* test/crypto: fix AES-ECB test lengths
* test/crypto: fix check for OOP header data
* test/crypto: remove unused variable
* test/dma: fix pointers in IOVA as PA mode
* test/event: fix number of queues in eventdev conf
* test/ring: fix init with custom number of lcores
* use Python raw string notation
* version: 24.11.2-rc1
* vhost: add null callback checks
* vhost: check descriptor chains length
* vhost: check GSO size validity
* vhost: clear ring addresses when getting vring base
* vhost/crypto: skip fetch before vring init
* vhost: fix FD entries cleanup
* vhost: fix log when setting max queue num
* vhost: reset packets count when not ready

24.11.2 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2025-April/052331.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 7.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2025-April/052414.html>`__

   * Basic functionality with testpmd

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-7
   * BlueField-2


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2025-April/052440.html>`__

   * Compile testing

   * Functional testing

      * PF (i40e, ixgbe)
      * VF (i40e, ixgbe)
      * PF/VF (ice)
      * IPsec
      * Virtio
      * Cryptodev
      * DLB

   * Performance testing

      * Throughput performance
      * Cryptodev latency
      * PF/VF NIC single core/NIC performance
      * XXV710/E810 NIC Performance

24.11.2 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 24.11.2 contains fixes from DPDK 25.03 and 8364a0f276eb ("net/ixgbe: fix VF registers for E610")
* Issues identified/fixed in DPDK main branch after DPDK 25.03 may be present in DPDK 24.11.2
* There is a report that https://bugs.dpdk.org/show_bug.cgi?id=1606 is still present despite the fix being backported. This needs further investigation for DPDK 24.11.3.

24.11.3 Release Notes
---------------------


24.11.3 Fixes
~~~~~~~~~~~~~

* acl: fix build with GCC 15 on aarch64
* app/crypto-perf: fix AAD offset alignment
* app/eventdev: fix number of releases sent during cleanup
* app/testpmd: fix flow random item token
* app/testpmd: fix RSS hash key update
* app/testpmd: relax number of TCs in DCB command
* buildtools/test: scan muti-line registrations
* bus/auxiliary: fix crash in cleanup
* bus: cleanup device lists
* bus/fslmc: fix use after free
* bus/pci/bsd: fix device existence check
* bus/vmbus: align ring buffer data to page boundary
* bus/vmbus: use Hyper-V page size
* common/cnxk: fix AES-CTR salt handling
* common/cnxk: fix aura offset
* common/cnxk: fix CQ tail drop
* common/cnxk: fix E-tag pattern parsing
* common/cnxk: fix qsize in CPT iq enable
* common/dpaax: fix PDCP AES only 12-bit SN
* common/dpaax: fix PDCP key command race condition
* common/mlx5: fix dependency detection on Windows
* common/mlx5: fix extraction of auxiliary device name
* crypto/cnxk: fix includes
* crypto/cnxk: fix out-of-bounds access in SM2
* crypto/cnxk: fix QP stats
* crypto/cnxk: fix uninitialized variable
* crypto/cnxk: update SG list population
* crypto/dpaa2_sec: fix uninitialized variable
* crypto/openssl: include private exponent in RSA session
* crypto/qat: fix out-of-place chain/cipher/auth headers
* crypto/qat: fix out-of-place chain/cipher/auth headers
* crypto/qat: fix out-of-place header bytes in AEAD raw API
* crypto/qat: fix size calculation for memset
* crypto/virtio: add request check on request side
* crypto/virtio: fix cipher data source length
* crypto/virtio: fix driver cleanup
* doc: add kernel options required for mlx5
* doc: fix missing feature matrix for event device
* doc: remove reference to deprecated --use-device option
* dts: fix deterministic doc
* eal: add description of service corelist in usage
* eal: fix return value of lcore role
* eal/freebsd: unregister alarm callback before free
* eal/linux: improve ASLR check
* eal/linux: unregister alarm callback before free
* eal/unix: fix log message for madvise failure
* eal: warn if no lcore is available
* eal/x86: fix C++ build
* ethdev: fix error struct in flow configure
* ethdev: keep promiscuous/allmulti value before disabling
* event/cnxk: fix missing HW state checks
* eventdev: fix flag types consistency
* event/dlb2: fix default credits based on HW version
* event/dlb2: fix dequeue with CQ depth <= 16
* event/dlb2: fix num single link ports for DLB2.5
* event/dlb2: fix public symbol namespace
* event/dlb2: fix QID depth xstat
* event/dlb2: fix validaton of LDB port COS ID arguments
* examples/flow_filtering: fix make clean
* examples/ipsec-secgw: fix crash in event vector mode
* examples/ipsec-secgw: fix crash with IPv6
* examples/ipsec-secgw: fix number of queue pairs
* examples/ntb: check more heap allocations
* mem: fix lockup on address space shortage
* net/af_xdp: fix use after free in zero-copy Tx
* net/bonding: avoid RSS RETA update in flow isolation mode
* net/cnxk: fix descriptor count update on reconfig
* net/cnxk: fix lock for security session operations
* net/e1000: fix EEPROM dump
* net/e1000: fix igb Tx queue offloads capability
* net/e1000: fix xstats name
* net/ena: fix aenq timeout with low poll interval
* net/ena: fix control path interrupt mode
* net: fix IPv6 check for IPv4 compat
* net/fm10k/base: fix compilation warnings
* net/hns3: allow Tx vector when fast free not enabled
* net/hns3: check requirement for hardware GRO
* net/hns3: fix CRC data segment
* net/hns3: fix divide by zero
* net/hns3: fix extra wait for link up
* net/hns3: fix integer overflow in interrupt unmap
* net/hns3: fix interrupt rollback
* net/hns3: fix memory leak for indirect flow action
* net/hns3: fix memory leak on failure
* net/hns3: fix queue TC configuration on VF
* net/hns3: fix resources release on reset
* net/hns3: fix Rx packet without CRC data
* net/i40e/base: fix compiler warnings
* net/i40e/base: fix unused value warnings
* net/i40e: fix RSS on plain IPv4
* net/iavf: fix VLAN strip disabling for ADQ v2 capability
* net/iavf: fix VLAN strip setting after enabling filter
* net/ice/base: fix integer overflow
* net/ice/base: fix media type check
* net/ice/base: fix type conversion
* net/ice/base: fix typo in device ID description
* net/ice: fix flow creation failure
* net/ice: fix handling empty DCF RSS hash
* net/ice: fix querying RSS hash for DCF
* net/ice: fix support for 3 scheduler levels
* net/idpf: fix truncation of constant value
* net/ixgbe/base: correct definition of endianness macro
* net/ixgbe/base: fix compilation warnings
* net/ixgbe/base: fix link status for E610
* net/ixgbe/base: fix lock checker errors
* net/ixgbe: enable ethertype filter for E610
* net/ixgbe: fix indentation
* net/ixgbe: fix port mask default value in filter
* net/ixgbe: remove VLAs
* net/ixgbe: skip MACsec stats for E610
* net/mana: check vendor ID when probing RDMA device
* net/mlx5: align PF and VF/SF MAC address handling
* net/mlx5: avoid setting kernel MTU if not needed
* net/mlx5: fix access to auxiliary flow data
* net/mlx5: fix counter pool init error propagation
* net/mlx5: fix counter service cleanup on init failure
* net/mlx5: fix crash in HWS counter pool destroy
* net/mlx5: fix crash on age query with indirect conntrack
* net/mlx5: fix error notification for large flow patterns
* net/mlx5: fix flex tunnel flow validation
* net/mlx5: fix GRE flow item validation
* net/mlx5: fix header modify action on group 0
* net/mlx5: fix hypervisor detection in VLAN workaround
* net/mlx5: fix link on Windows
* net/mlx5: fix mark action with shared Rx queue
* net/mlx5: fix masked indirect age action validation
* net/mlx5: fix maximal queue size query
* net/mlx5: fix out-of-order completions in ordinary Rx burst
* net/mlx5: fix template flow rule identification
* net/mlx5: fix validation for GENEVE options
* net/mlx5: fix VLAN stripping on hairpin queue
* net/mlx5: fix WQE size calculation for Tx queue
* net/mlx5/hws: fix send queue drain on FW WQE destroy
* net/mlx5: remove unsupported flow meter action in HWS
* net/mlx5: validate GTP PSC QFI width
* net/netvsc: add stats counters from VF
* net/netvsc: use Hyper-V page size
* net/nfp: fix control message overflow
* net/nfp: fix crash with null RSS hash key
* net/nfp: fix flow rule freeing
* net/nfp: fix hash key length logic
* net/nfp: standardize NFD3 Tx descriptor endianness
* net/nfp: standardize NFDk Tx descriptor endianness
* net/nfp: standardize Rx descriptor endianness
* net/ngbe: fix device statistics
* net/ngbe: fix MAC control frame forwarding
* net/ntnic: avoid divide by zero
* net/ntnic: fix ring queue operation
* net/ntnic: remove unused code
* net/ntnic: unmap DMA during queue release
* net/null: fix packet copy
* net/octeon_ep: fix buffer refill
* net/octeon_ep: increase mailbox timeout
* net/qede: fix use after free
* net/sfc: fix action order on start failure
* net/tap: fix qdisc add failure handling
* net/txgbe: add LRO flag in mbuf when enabled
* net/txgbe: fix device statistics
* net/txgbe: fix FDIR perfect mode for IPv6
* net/txgbe: fix MAC control frame forwarding
* net/txgbe: fix ntuple filter parsing
* net/txgbe: fix packet type for FDIR filter
* net/txgbe: fix raw pattern match for FDIR rule
* net/txgbe: fix reserved extra FDIR headroom
* net/txgbe: fix to create FDIR filter for SCTP packet
* net/txgbe: fix to create FDIR filter for tunnel packet
* net/txgbe: restrict VLAN strip configuration on VF
* pcapng: fix null dereference in close
* power/intel_uncore: fix crash closing uninitialized driver
* test/crypto: fix auth and cipher case IV length
* test/crypto: fix EdDSA vector description
* test/crypto: fix RSA decrypt validation
* test/crypto: fix RSA vector as per RFC 8017
* test/crypto: set to null after freeing operation
* test/lcore: fix race in per-lcore test
* test/malloc: improve resiliency
* trace: fix overflow in per-lcore trace buffer
* version: 24.11.3-rc1
* vhost/crypto: fix cipher data length
* vhost: fix net control virtqueue used length
* vhost: fix wrapping on control virtqueue rings
* vhost: search virtqueues driver data in read-only area

24.11.3 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2025-July/053454.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 9.1.0
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2025-August/053692.html>`__

   * Basic functionality with testpmd

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-7
   * BlueField-2


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2025-August/053672.html>`__

   * Compile testing

   * Functional testing

      * PF (i40e, ixgbe)
      * VF (i40e, ixgbe)
      * PF/VF (ice)
      * IPsec
      * Virtio
      * Cryptodev
      * DLB

   * Performance testing

      * Throughput performance
      * Cryptodev latency
      * PF/VF NIC single core/NIC performance
      * XXV710/E810 NIC Performance

24.11.3 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 24.11.3 contains fixes from DPDK 25.07
* Issues identified/fixed in DPDK main branch after DPDK 25.07 may be present in DPDK 24.11.3
* `Bug 1606 - flow_filtering/flow_filtering_mismatch_rule: Some mismatch rule packets match the rule. <https://bugs.dpdk.org/show_bug.cgi?id=1606>`__ reported by Intel validation team, despite listed fix being present.

24.11.3 Fixes skipped and status unresolved
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* 9843181aa5  crypto/virtio: fix driver ID
* 25e35296b5  net/cnxk: fix reconfiguring MAC address
* 98cf04bb90  test/graph: fix second run

24.11.4 Release Notes
---------------------


24.11.4 Fixes
~~~~~~~~~~~~~

* app/crypto-perf: fix plaintext size exceeds buffer size
* app/dma-perf: check buffer size
* app/dma-perf: fix on-flight DMA when verifying data
* app/dma-perf: fix stopping device
* app/dma-perf: fix use after free
* app/eventdev: fix build with clang 21
* app/flow-perf: fix rules array length
* app/pdump: remove hard-coded memory channels
* app/procinfo: fix const pointer in collectd format
* app/testpmd: fix conntrack action query
* app/testpmd: fix DCB Rx queues
* app/testpmd: fix DCB Tx port
* app/testpmd: fix flex item link parsing
* app/testpmd: fix mask in flow random item
* app/testpmd: increase size of set cores list command
* app/testpmd: monitor state of primary process
* app/testpmd: stop forwarding in secondary process
* app/testpmd: validate DSCP and VLAN for meter creation
* bbdev: fix build with MinGW 13
* bitops: improve power of 2 alignment documentation
* buildtools/pmdinfogen: fix warning with python 3.14
* bus/cdx: fix device name in probing error message
* bus/cdx: fix release in probing for secondary process
* bus/dpaa: improve cleanup
* bus/fslmc: fix const pointer in device name parsing
* bus/ifpga: fix const pointer in device name parsing
* bus/pci: fix build with MinGW 13
* bus/pci: fix resource leak in secondary process
* bus/uacce: fix const pointer in device matching
* cmdline: fix highest bit port list parsing
* cmdline: fix port list parsing
* common/cnxk: fix async event handling
* common/cnxk: fix format specifier for bandwidth profile ID
* common/cnxk: fix max number of SQB buffers in clean up
* common/cnxk: fix NIX Rx inject enabling
* common/cnxk: fix null SQ access
* common/mlx5: release unused mempool entries
* common/mlx5: remove useless constants
* common/qat: fix some const pointers
* config/arm: enable NUMA for Neoverse N2
* crypto/caam_jr: fix const pointer in UIO filename parsing
* crypto/cnxk: refactor RSA verification
* crypto/ipsec_mb: fix QP release in secondary
* crypto/mlx5: remove unused constants
* crypto/qat: fix CCM request descriptor hash state size
* crypto/qat: fix ECDH
* crypto/qat: fix source buffer alignment
* crypto/virtio: fix cookies leak
* dmadev: fix debug build with tracepoints
* dma/hisilicon: fix stop with pending transfers
* doc: add conntrack state inspect command to testpmd guide
* doc: add Pollara 400 device in ionic guide
* doc: fix display of commands in cpfl guide
* doc: fix feature list of iavf driver
* doc: fix feature list of ice driver
* doc: fix note in FreeBSD guide
* doc: fix NVIDIA bifurcated driver presentation link
* dts: add reception check to checksum suite
* dts: fix docstring in checksum suite
* eal/arm: fix C++ build for 32-bit memcpy
* eal: fix DMA mask validation with IOVA mode option
* eal: fix MP socket cleanup
* eal: fix plugin dir walk
* eal/x86: enable timeout in AMD power monitor
* efd: fix AVX2 support
* ethdev: fix VLAN filter parameter description
* eventdev/crypto: fix build with clang 21
* eventdev: fix listing timer adapters with telemetry
* examples/l3fwd: add Tx burst size configuration option
* examples/l3fwd-power: fix telemetry command registration
* examples/server_node_efd: fix format overflow
* examples/vdpa: fix format overflow
* fib6: fix memory leak on delete operation
* fib6: fix tbl8 allocation check logic
* graph: fix stats query with no node xstats
* graph: fix unaligned access in stats
* graph: fix updating edge with active graph
* graph: fix xstats description allocation
* gro: fix payload corruption in coalescing packets
* hash: fix unaligned access in predictable RSS
* lib: fix backticks matching in Doxygen comments
* mcslock: fix memory ordering
* net/af_packet: fix crash in secondary process
* net/ark: remove double mbuf free
* net/axgbe: fix build with GCC 16
* net/bnxt: fix free of not allocated object
* net/bonding: fix MAC address propagation in 802.3ad mode
* net/cnxk: fix default meter pre-color
* net/cnxk: fix Rx inject LF
* net/dpaa2: clear active VDQ state when freeing Rx queues
* net/dpaa2: fix duplicate call of close
* net/dpaa2: fix error frame dump
* net/dpaa2: fix extract buffer preparation
* net/dpaa2: fix flow rule resizing
* net/dpaa2: fix L3/L4 checksum results
* net/dpaa2: fix shaper rate
* net/dpaa2: fix uninitialized variable
* net/dpaa2: free buffers from error queue
* net/dpaa2: receive packets with additional parse errors
* net/dpaa2: remove ethdev pointer from bus device
* net/dpaa: fix resource leak
* net/e1000/base: fix crash on init with GCC 13
* net/ena/base: fix unsafe memcpy on invalid memory
* net/ena: fix PCI BAR mapping on 64K page size
* net/enetfec: fix buffer descriptor size configuration
* net/enetfec: fix checksum flag handling and error return
* net/enetfec: fix const pointer in UIO filename parsing
* net/enetfec: fix file descriptor leak on read error
* net/enetfec: fix memory leak in Rx buffer cleanup
* net/enetfec: fix out-of-bounds access in UIO mapping
* net/enetfec: fix Tx queue free
* net/enetfec: reject multi-queue configuration
* net/enetfec: reject Tx deferred queue
* net: fix IPv6 link local compliance with RFC 4291
* net: fix L2 length for GRE packets
* net/fm10k: fix build with GCC 16
* net/gve: add DQO Tx descriptor limit
* net/gve: clean when insufficient Tx descriptors
* net/gve: clear DQO Tx descriptors before writing
* net/gve: do not write zero-length descriptors
* net/gve: fix disabling interrupts on DQ
* net/gve: fix DQO TSO descriptor limit
* net/gve: free device resources on close
* net/gve: free Rx mbufs if allocation fails on ring setup
* net/gve: send whole packet when mbuf is large
* net/gve: validate Tx packet before sending
* net/hns3: fix inconsistent lock
* net/hns3: fix overwrite mbuf in vector path
* net/hns3: fix VLAN resources freeing
* net/hns3: fix VLAN tag loss for short tunnel frame
* net/i40e: fix symmetric Toeplitz hashing for SCTP
* net/iavf: fix build with clang 21
* net/iavf: fix Rx timestamp validity check
* net/iavf: fix Tx vector path selection logic
* net/ice/base: fix adding special words
* net/ice/base: fix integer overflow on NVM init
* net/ice/base: fix memory leak in HW profile handling
* net/ice/base: fix memory leak in recipe handling
* net/ice: fix initialization with 8 ports
* net/ice: fix memory leak in raw pattern parse
* net/ice: fix path selection for QinQ Tx offload
* net/ice: fix statistics
* net/ice: fix vector Rx VLAN offload flags
* net/ice: fix VLAN tag reporting on Rx
* net/ice: remove indirection for FDIR filters
* net/ice: remove unsupported SCTP Rx offload
* net/idpf: fix queue setup with TSO offload
* net/intel: fix assumption about tag placement order
* net/ixgbe/base: fix PF link state request size
* net/ixgbe: fix SCTP port filtering on E610
* net/memif: fix const pointer in socket check
* net/mlx4: fix unnecessary comma
* net/mlx5: fix build with MinGW 13
* net/mlx5: fix connection tracking state item validation
* net/mlx5: fix crash on flow rule destruction
* net/mlx5: fix default flow rules start
* net/mlx5: fix device start error handling
* net/mlx5: fix Direct Verbs counter offset detection
* net/mlx5: fix double free in non-template flow destroy
* net/mlx5: fix error reporting on masked indirect actions
* net/mlx5: fix ESP header match after UDP for group 0
* net/mlx5: fix ESP item validation to match on seqnum
* net/mlx5: fix external queues access
* net/mlx5: fix flex flow item header length
* net/mlx5: fix flow aging race condition
* net/mlx5: fix flow encapsulation hash
* net/mlx5: fix flow tag indexes support on root table
* net/mlx5: fix index-based flow rules
* net/mlx5: fix indirect flow action memory leak
* net/mlx5: fix indirect flow age action handling
* net/mlx5: fix indirect meter index leak
* net/mlx5: fix indirect RSS action hash
* net/mlx5: fix interface name parameter definition
* net/mlx5: fix IPv6 DSCP offset in HWS sync API
* net/mlx5: fix leak of flow indexed pools
* net/mlx5: fix meter mark allocation
* net/mlx5: fix min and max MTU reporting
* net/mlx5: fix modify field action restriction
* net/mlx5: fix MTU initialization
* net/mlx5: fix multicast
* net/mlx5: fix multi-process Tx default rules
* net/mlx5: fix non-template age rules flush
* net/mlx5: fix non-template RSS expansion
* net/mlx5: fix null dereference in modify header
* net/mlx5: fix send to kernel action resources release
* net/mlx5: fix spurious CPU wakeups
* net/mlx5: fix storage of shared Rx queues
* net/mlx5: fix Tx metadata pattern template mismatch
* net/mlx5: fix uninitialized variable
* net/mlx5: fix unnecessary commas
* net/mlx5: fix unsupported flow rule port action
* net/mlx5: fix use after scope of RSS configuration
* net/mlx5/hws: fix buddy memory allocation
* net/mlx5/hws: fix ESP header match in strict mode
* net/mlx5/hws: fix flow rule hash capability
* net/mlx5/hws: fix TIR action support in FDB
* net/mlx5: move auxiliary data inline
* net/mlx5: release representor interrupt handler
* net/mlx5: remove counter alignment
* net/mlx5: remove unused macros
* net/mlx5: remove useless constants
* net/mlx5: skip Rx control flow tables in isolated mode
* net/mlx5: store MTU at Rx queue allocation time
* net/mlx5/windows: fix match criteria in flow creation
* net/nfp: fix metering cleanup
* net/ngbe: fix checksum error counter
* net/ngbe: reduce memory size of ring descriptors
* net/ntnic: fix potential format overflow
* net/octeon_ep: fix device start
* net/octeon_ep: fix mbuf data offset update
* net/octeon_ep: handle interrupt enable failure
* net/tap: fix BPF with cross-compilation
* net/tap: fix build with LTO
* net/tap: fix interrupt callback crash after failed start
* net/txgbe: add device arguments for FDIR
* net/txgbe: filter FDIR match flex bytes for tunnel
* net/txgbe: fix checksum error counter
* net/txgbe: fix FDIR drop action for L4 match packets
* net/txgbe: fix FDIR filter for SCTP tunnel
* net/txgbe: fix FDIR input mask
* net/txgbe: fix FDIR mode clearing
* net/txgbe: fix FDIR rule raw relative for L3 packets
* net/txgbe: fix maximum number of FDIR filters
* net/txgbe: fix VF Rx buffer size in config register
* net/txgbe: reduce memory size of ring descriptors
* net/txgbe: remove duplicate Tx queue assignment
* net/txgbe: remove unsupported flow action mark
* net/txgbe: switch to FDIR when ntuple filter is full
* net/virtio-user: fix used ring address calculation
* net/vmxnet3: disable RSS for single queue for ESX8.0+
* net/vmxnet3: fix mapping of mempools to queues
* net/zxdh: fix Arm build
* pdump: handle primary process exit
* rawdev: fix build with clang 21
* regex/mlx5: remove useless constants
* Revert "crypto/virtio: fix cookies leak"
* ring: establish a safe partial order in hts-ring
* ring: establish safe partial order in default mode
* ring: establish safe partial order in RTS mode
* sched: fix WRR parameter data type
* tailq: fix lookup macro
* telemetry: make socket handler typedef private
* test/argparse: change initialization to workaround LTO
* test/crypto: fix mbuf handling
* test/crypto: fix vector initialization
* test/debug: fix crash with mlx5 devices
* test/debug: fix IOVA mode on PPC64 without huge pages
* test/dma: fix failure condition
* test: fix build with clang 21
* test/func_reentrancy: fix args to EAL init call
* test/hash: check memory allocation
* test/telemetry: fix test calling all commands
* usertools/telemetry: fix exporter default IP binding
* vdpa/mlx5: remove unused constant
* version: 24.11.4-rc1
* vhost: add VDUSE virtqueue ready state polling workaround
* vhost: fix double fetch when dequeue offloading
* vhost: fix external buffer in VDUSE
* vhost: fix virtqueue info init in VDUSE vring setup

24.11.4 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2025-December/055094.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 8.2.0
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2025-December/055146.html>`__

   * Compile testing

   * Functional testing

      * PF (i40e, ixgbe)
      * VF (i40e, ixgbe)
      * PF/VF (ice)
      * IPsec
      * Virtio
      * Cryptodev
      * DLB

   * Performance testing

      * Throughput performance
      * Cryptodev latency
      * PF/VF NIC single core/NIC performance
      * XXV710/E810 NIC Performance


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2025-December/055164.html>`__

   * Basic functionality with testpmd

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx

24.11.4 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 24.11.4 contains fixes from DPDK 25.11
* Issues identified/fixed in DPDK main branch after DPDK 25.11 may be present in DPDK 24.11.4
* `Bug 1855 - E610 vfs can't forward packets. <https://bugs.dpdk.org/show_bug.cgi?id=1855>`__ reported by Intel validation team.

24.11.4 Fixes skipped and status unresolved
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* 0e3c389b9e  bus/dpaa: do proper cleanup of frame queues on shutdown
* 08b83e84e1  crypto/cnxk: fix TLS mbuf
* 9843181aa5  crypto/virtio: fix driver ID
* 3e48adc135  event/cnxk: fix Rx offload flags
* 5a753913e0  Kushwaha common/cnxk: fix inline device write operation
* 25e35296b5  net/cnxk: fix reconfiguring MAC address
* 3bf9f0f9f0  net/mlx5: fix control flow leakage for external SQ
* f2f75ffe14  net/mlx5/hws: fix ESP header match in strict mode
* 98cf04bb90  test/graph: fix second run
* 8a8c02d2bb  vfio: fix custom containers in multiprocess
