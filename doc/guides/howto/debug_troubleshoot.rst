..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Debug & Troubleshoot guide
==========================

DPDK applications can be designed to have simple or complex pipeline processing
stages making use of single or multiple threads. Applications can use poll mode
hardware devices which helps in offloading CPU cycles too. It is common to find
solutions designed with

* single or multiple primary processes

* single primary and single secondary

* single primary and multiple secondaries

In all the above cases, it is tedious to isolate, debug, and understand various
behaviors which occur randomly or periodically. The goal of the guide is to
consolidate a few commonly seen issues for reference. Then, isolate to identify
the root cause through step by step debug at various stages.

.. note::

 It is difficult to cover all possible issues; in a single attempt. With
 feedback and suggestions from the community, more cases can be covered.


Application Overview
--------------------

By making use of the application model as a reference, we can discuss multiple
causes of issues in the guide. Let us assume the sample makes use of a single
primary process, with various processing stages running on multiple cores. The
application may also make uses of Poll Mode Driver, and libraries like service
cores, mempool, mbuf, eventdev, cryptodev, QoS, and ethdev.

The overview of an application modeled using PMD is shown in
:numref:`dtg_sample_app_model`.

.. _dtg_sample_app_model:

.. figure:: img/dtg_sample_app_model.*

   Overview of pipeline stage of an application


Bottleneck Analysis
-------------------

A couple of factors that lead the design decision could be the platform, scale
factor, and target. This distinct preference leads to multiple combinations,
that are built using PMD and libraries of DPDK. While the compiler, library
mode, and optimization flags are the components are to be constant, that
affects the application too.


Is there mismatch in packet (received < desired) rate?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

RX Port and associated core :numref:`dtg_rx_rate`.

.. _dtg_rx_rate:

.. figure:: img/dtg_rx_rate.*

   RX packet rate compared against received rate.

#. Is the configuration for the RX setup correctly?

   * Identify if port Speed and Duplex is matching to desired values with
     ``rte_eth_link_get``.

   * Check ``DEV_RX_OFFLOAD_JUMBO_FRAME`` is set with ``rte_eth_dev_info_get``.

   * Check promiscuous mode if the drops do not occur for unique MAC address
     with ``rte_eth_promiscuous_get``.

#. Is the drop isolated to certain NIC only?

   * Make use of ``rte_eth_dev_stats`` to identify the drops cause.

   * If there are mbuf drops, check nb_desc for RX descriptor as it might not
     be sufficient for the application.

   * If ``rte_eth_dev_stats`` shows drops are on specific RX queues, ensure RX
     lcore threads has enough cycles for ``rte_eth_rx_burst`` on the port queue
     pair.

   * If there are redirect to a specific port queue pair with, ensure RX lcore
     threads gets enough cycles.

   * Check the RSS configuration ``rte_eth_dev_rss_hash_conf_get`` if the
     spread is not even and causing drops.

   * If PMD stats are not updating, then there might be offload or configuration
     which is dropping the incoming traffic.

#. Is there drops still seen?

   * If there are multiple port queue pair, it might be the RX thread, RX
     distributor, or event RX adapter not having enough cycles.

   * If there are drops seen for RX adapter or RX distributor, try using
     ``rte_prefetch_non_temporal`` which intimates the core that the mbuf in the
     cache is temporary.


Is there packet drops at receive or transmit?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

RX-TX port and associated cores :numref:`dtg_rx_tx_drop`.

.. _dtg_rx_tx_drop:

.. figure:: img/dtg_rx_tx_drop.*

   RX-TX drops

#. At RX

   * Identify if there are multiple RX queue configured for port by
     ``nb_rx_queues`` using ``rte_eth_dev_info_get``.

   * Using ``rte_eth_dev_stats`` fetch drops in q_errors, check if RX thread
     is configured to fetch packets from the port queue pair.

   * Using ``rte_eth_dev_stats`` shows drops in ``rx_nombuf``, check if RX
     thread has enough cycles to consume the packets from the queue.

#. At TX

   * If the TX rate is falling behind the application fill rate, identify if
     there are enough descriptors with ``rte_eth_dev_info_get`` for TX.

   * Check the ``nb_pkt`` in ``rte_eth_tx_burst`` is done for multiple packets.

   * Check ``rte_eth_tx_burst`` invokes the vector function call for the PMD.

   * If oerrors are getting incremented, TX packet validations are failing.
     Check if there queue specific offload failures.

   * If the drops occur for large size packets, check MTU and multi-segment
     support configured for NIC.


Is there object drops in producer point for the ring library?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Producer point for ring :numref:`dtg_producer_ring`.

.. _dtg_producer_ring:

.. figure:: img/dtg_producer_ring.*

   Producer point for Rings

#. Performance issue isolation at producer

   * Use ``rte_ring_dump`` to validate for all single producer flag is set to
     ``RING_F_SP_ENQ``.

   * There should be sufficient ``rte_ring_free_count`` at any point in time.

   * Extreme stalls in dequeue stage of the pipeline will cause
     ``rte_ring_full`` to be true.


Is there object drops in consumer point for the ring library?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Consumer point for ring :numref:`dtg_consumer_ring`.

.. _dtg_consumer_ring:

.. figure:: img/dtg_consumer_ring.*

   Consumer point for Rings

#. Performance issue isolation at consumer

   * Use ``rte_ring_dump`` to validate for all single consumer flag is set to
     ``RING_F_SC_DEQ``.

   * If the desired burst dequeue falls behind the actual dequeue, the enqueue
     stage is not filling up the ring as required.

   * Extreme stall in the enqueue will lead to ``rte_ring_empty`` to be true.


Is there a variance in packet or object processing rate in the pipeline?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Memory objects close to NUMA :numref:`dtg_mempool`.

.. _dtg_mempool:

.. figure:: img/dtg_mempool.*

   Memory objects have to be close to the device per NUMA.

#. Stall in processing pipeline can be attributes of MBUF release delays.
   These can be narrowed down to

   * Heavy processing cycles at single or multiple processing stages.

   * Cache is spread due to the increased stages in the pipeline.

   * CPU thread responsible for TX is not able to keep up with the burst of
     traffic.

   * Extra cycles to linearize multi-segment buffer and software offload like
     checksum, TSO, and VLAN strip.

   * Packet buffer copy in fast path also results in stalls in MBUF release if
     not done selectively.

   * Application logic sets ``rte_pktmbuf_refcnt_set`` to higher than the
     desired value and frequently uses ``rte_pktmbuf_prefree_seg`` and does
     not release MBUF back to mempool.

#. Lower performance between the pipeline processing stages can be

   * The NUMA instance for packets or objects from NIC, mempool, and ring
     should be the same.

   * Drops on a specific socket are due to insufficient objects in the pool.
     Use ``rte_mempool_get_count`` or ``rte_mempool_avail_count`` to monitor
     when drops occurs.

   * Try prefetching the content in processing pipeline logic to minimize the
     stalls.

#. Performance issue can be due to special cases

   * Check if MBUF continuous with ``rte_pktmbuf_is_contiguous`` as certain
     offload requires the same.

   * Use ``rte_mempool_cache_create`` for user threads require access to
     mempool objects.

   * If the variance is absent for larger huge pages, then try rte_mem_lock_page
     on the objects, packets, lookup tables to isolate the issue.


Is there a variance in cryptodev performance?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Crypto device and PMD :numref:`dtg_crypto`.

.. _dtg_crypto:

.. figure:: img/dtg_crypto.*

   CRYPTO and interaction with PMD device.

#. Performance issue isolation for enqueue

   * Ensure cryptodev, resources and enqueue is running on NUMA cores.

   * Isolate if the cause of errors for err_count using ``rte_cryptodev_stats``.

   * Parallelize enqueue thread for varied multiple queue pair.

#. Performance issue isolation for dequeue

   * Ensure cryptodev, resources and dequeue are running on NUMA cores.

   * Isolate if the cause of errors for err_count using ``rte_cryptodev_stats``.

   * Parallelize dequeue thread for varied multiple queue pair.

#. Performance issue isolation for crypto operation

   * If the cryptodev software-assist is in use, ensure the library is built
     with right (SIMD) flags or check if the queue pair using CPU ISA for
     feature_flags AVX|SSE|NEON using ``rte_cryptodev_info_get``.

   * If the cryptodev hardware-assist is in use, ensure both firmware and
     drivers are up to date.

#. Configuration issue isolation

   * Identify cryptodev instances with ``rte_cryptodev_count`` and
     ``rte_cryptodev_info_get``.


Is user functions performance is not as expected?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Custom worker function :numref:`dtg_distributor_worker`.

.. _dtg_distributor_worker:

.. figure:: img/dtg_distributor_worker.*

   Custom worker function performance drops.

#. Performance issue isolation

   * The functions running on CPU cores without context switches are the
     performing scenarios. Identify lcore with ``rte_lcore`` and lcore index
     mapping with CPU using ``rte_lcore_index``.

   * Use ``rte_thread_get_affinity`` to isolate functions running on the same
     CPU core.

#. Configuration issue isolation

   * Identify core role using ``rte_eal_lcore_role`` to identify RTE, OFF and
     SERVICE. Check performance functions are mapped to run on the cores.

   * For high-performance execution logic ensure running it on correct NUMA
     and non-master core.

   * Analyze run logic with ``rte_dump_stack``, ``rte_dump_registers`` and
     ``rte_memdump`` for more insights.

   * Make use of objdump to ensure opcode is matching to the desired state.


Is the execution cycles for dynamic service functions are not frequent?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

service functions on service cores :numref:`dtg_service`.

.. _dtg_service:

.. figure:: img/dtg_service.*

   functions running on service cores

#. Performance issue isolation

   * Services configured for parallel execution should have
     ``rte_service_lcore_count`` should be equal to
     ``rte_service_lcore_count_services``.

   * A service to run parallel on all cores should return
     ``RTE_SERVICE_CAP_MT_SAFE`` for ``rte_service_probe_capability`` and
     ``rte_service_map_lcore_get`` returns unique lcore.

   * If service function execution cycles for dynamic service functions are
     not frequent?

   * If services share the lcore, overall execution should fit budget.

#. Configuration issue isolation

   * Check if service is running with ``rte_service_runstate_get``.

   * Generic debug via ``rte_service_dump``.


Is there a bottleneck in the performance of eventdev?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. Check for generic configuration

   * Ensure the event devices created are right NUMA using
     ``rte_event_dev_count`` and ``rte_event_dev_socket_id``.

   * Check for event stages if the events are looped back into the same queue.

   * If the failure is on the enqueue stage for events, check if queue depth
     with ``rte_event_dev_info_get``.

#. If there are performance drops in the enqueue stage

   * Use ``rte_event_dev_dump`` to dump the eventdev information.

   * Periodically checks stats for queue and port to identify the starvation.

   * Check the in-flight events for the desired queue for enqueue and dequeue.


Is there a variance in traffic manager?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Traffic Manager on TX interface :numref:`dtg_qos_tx`.

.. _dtg_qos_tx:

.. figure:: img/dtg_qos_tx.*

   Traffic Manager just before TX.

#. Identify the cause for a variance from expected behavior, is due to
   insufficient CPU cycles. Use ``rte_tm_capabilities_get`` to fetch features
   for hierarchies, WRED and priority schedulers to be offloaded hardware.

#. Undesired flow drops can be narrowed down to WRED, priority, and rates
   limiters.

#. Isolate the flow in which the undesired drops occur. Use
   ``rte_tn_get_number_of_leaf_node`` and flow table to ping down the leaf
   where drops occur.

#. Check the stats using ``rte_tm_stats_update`` and ``rte_tm_node_stats_read``
   for drops for hierarchy, schedulers and WRED configurations.


Is the packet in the unexpected format?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Packet capture before and after processing :numref:`dtg_pdump`.

.. _dtg_pdump:

.. figure:: img/dtg_pdump.*

   Capture points of Traffic at RX-TX.

#. To isolate the possible packet corruption in the processing pipeline,
   carefully staged capture packets are to be implemented.

   * First, isolate at NIC entry and exit.

     Use pdump in primary to allow secondary to access port-queue pair. The
     packets get copied over in RX|TX callback by the secondary process using
     ring buffers.

   * Second, isolate at pipeline entry and exit.

     Using hooks or callbacks capture the packet middle of the pipeline stage
     to copy the packets, which can be shared to the secondary debug process
     via user-defined custom rings.

.. note::

   Use similar analysis to objects and metadata corruption.


Does the issue still persist?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The issue can be further narrowed down to the following causes.

#. If there are vendor or application specific metadata, check for errors due
   to META data error flags. Dumping private meta-data in the objects can give
   insight into details for debugging.

#. If there are multi-process for either data or configuration, check for
   possible errors in the secondary process where the configuration fails and
   possible data corruption in the data plane.

#. Random drops in the RX or TX when opening other application is an indication
   of the effect of a noisy neighbor. Try using the cache allocation technique
   to minimize the effect between applications.


How to develop a custom code to debug?
--------------------------------------

#. For an application that runs as the primary process only, debug functionality
   is added in the same process. These can be invoked by timer call-back,
   service core and signal handler.

#. For the application that runs as multiple processes. debug functionality in
   a standalone secondary process.
