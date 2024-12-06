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
