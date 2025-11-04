..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

Packet Capture Library
======================

The DPDK ``pdump`` library provides a framework
for capturing packets within DPDK applications.
It enables a **secondary process** to monitor packets
being processed by both **primary** or **secondary** processes.


Overview
--------

The library uses a multi-process channel
to facilitate communication between the primary and secondary processes.
This mechanism allows enabling or disabling packet capture
on specific ports or queues.

.. _figure_pdump_overview:

.. figure:: img/pdump_overview.*

   Packet Capture enable and disable sequence


API Reference
-------------

The library exposes API for:

* Initializing and uninitializing the packet capture framework.
* Enabling and disabling packet capture.
* Applying optional filters and limiting captured packet length.


.. function:: int rte_pdump_init(void)

   Initialize the packet capture framework.

.. function:: int rte_pdump_enable(uint16_t port_id, uint16_t queue, uint32_t flags)

   Enable packet capture on the specified port and queue.

.. function:: int rte_pdump_enable_bpf(uint16_t port_id, uint16_t queue, const struct rte_bpf_program *bpf, uint32_t snaplen)

   Enable packet capture on the specified port and queue
   with an optional BPF packet filter
   and a limit on the captured packet length.

.. function:: int rte_pdump_enable_by_deviceid(const char *device_id, uint16_t queue, uint32_t flags)

   Enable packet capture on the specified device ID (``vdev`` name or PCI address)
   and queue.

.. function:: int rte_pdump_enable_bpf_by_deviceid(const char *device_id, uint16_t queue, const struct rte_bpf_program *bpf, uint32_t snaplen)

   Enable packet capture on the specified device ID (``vdev`` name or PCI address)
   and queue, with optional filtering and captured packet length limit.

.. function:: int rte_pdump_disable(uint16_t port_id, uint16_t queue)

   Disable packet capture on the specified port and queue.
   This applies to the current process and all other processes.

.. function:: int rte_pdump_disable_by_deviceid(const char *device_id, uint16_t queue)

   Disable packet capture on the specified device ID (``vdev`` name or PCI address)
   and queue.

.. function:: int rte_pdump_uninit(void)

   Uninitialize the packet capture framework for this process.

.. function:: int rte_pdump_stats(uint16_t port_id, struct rte_dump_stats *stats)

   Reports the number of packets captured, filtered, and missed.
   Packets maybe missed due to mbuf pool being exhausted or the ring being full.


Operation
---------

All processes using ``librte_pdump`` must initialize
the packet capture framework before use.
This initialization is required in both the primary and secondary processes.

DPDK provides the following utilities that use this library:

* ``dpdk-dumpcap``
* ``dpdk-pdump``


Implementation Details
----------------------

``rte_pdump_init()`` creates the multi-process channel
by calling ``rte_mp_action_register()``.

The primary process listens for requests from secondary processes
to enable or disable packet capture over the multi-process channel.

When a secondary process calls ``rte_pdump_enable()``
or ``rte_pdump_enable_by_deviceid()``,
the library sends a "pdump enable" request
to the primary process.
The primary process then:

#. Receives the request over the multi-process channel.
#. Registers Ethernet Rx and Tx callbacks for the specified port.
#. Forwards the request to other secondary processes (if any).


FAQ
---

What is the performance impact of pdump?

   Setting up pdump with ``rte_pdump_init`` has no impact,
   there are no changes in the fast path.
   When pdump is enabled, the Tx and Rx fast path functions
   callbacks make a copy of the mbufs and enqueue them.
   This will impact performance.
   The effect can be reduced by filtering
   to only see the packets of interest
   and using the ``snaplen`` parameter to only copy the needed headers.

What happens if process does not call pdump init?

   If application does not call ``rte_pdump_init``
   then the request to enable (in the capture command)
   will timeout and an error is returned.

Where do packets go?

   Packets captured are placed in the ring passed in ``rte_pdump_enable``.
   The capture application must dequeue these mbuf's and free them.

Why is copy required?

   A copy is used instead of incrementing the reference count
   because on transmit the device may be using fast free which does not use refcounts;
   and on receive the application may modify the incoming packet.

What about offloads?

   The offload flags of the original mbuf are copied to the ring.
   It is up to the capture application to handle flags like VLAN stripping
   as necessary.
   Packets are captured before passing to driver and hardware
   so the actual packet on the wire may be segmented or encapsulated
   based on the offload flags.
