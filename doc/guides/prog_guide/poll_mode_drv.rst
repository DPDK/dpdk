..  BSD LICENSE
    Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
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

.. _Poll_Mode_Driver:

Poll Mode Driver
================

The DPDK includes 1 Gigabit, 10 Gigabit and 40 Gigabit and para virtualized virtio Poll Mode Drivers.

A Poll Mode Driver (PMD) consists of APIs, provided through the BSD driver running in user space,
to configure the devices and their respective queues.
In addition, a PMD accesses the RX and TX descriptors directly without any interrupts
(with the exception of Link Status Change interrupts) to quickly receive,
process and deliver packets in the user's application.
This section describes the requirements of the PMDs,
their global design principles and proposes a high-level architecture and a generic external API for the Ethernet PMDs.

Requirements and Assumptions
----------------------------

The DPDK environment for packet processing applications allows for two models, run-to-completion and pipe-line:

*   In the *run-to-completion*  model, a specific port's RX descriptor ring is polled for packets through an API.
    Packets are then processed on the same core and placed on a port's TX descriptor ring through an API for transmission.

*   In the *pipe-line*  model, one core polls one or more port's RX descriptor ring through an API.
    Packets are received and passed to another core via a ring.
    The other core continues to process the packet which then may be placed on a port's TX descriptor ring through an API for transmission.

In a synchronous run-to-completion model,
each logical core assigned to the DPDK executes a packet processing loop that includes the following steps:

*   Retrieve input packets through the PMD receive API

*   Process each received packet one at a time, up to its forwarding

*   Send pending output packets through the PMD transmit API

Conversely, in an asynchronous pipe-line model, some logical cores may be dedicated to the retrieval of received packets and
other logical cores to the processing of previously received packets.
Received packets are exchanged between logical cores through rings.
The loop for packet retrieval includes the following steps:

*   Retrieve input packets through the PMD receive API

*   Provide received packets to processing lcores through packet queues

The loop for packet processing includes the following steps:

*   Retrieve the received packet from the packet queue

*   Process the received packet, up to its retransmission if forwarded

To avoid any unnecessary interrupt processing overhead, the execution environment must not use any asynchronous notification mechanisms.
Whenever needed and appropriate, asynchronous communication should be introduced as much as possible through the use of rings.

Avoiding lock contention is a key issue in a multi-core environment.
To address this issue, PMDs are designed to work with per-core private resources as much as possible.
For example, a PMD maintains a separate transmit queue per-core, per-port, if the PMD is not ``DEV_TX_OFFLOAD_MT_LOCKFREE`` capable.
In the same way, every receive queue of a port is assigned to and polled by a single logical core (lcore).

To comply with Non-Uniform Memory Access (NUMA), memory management is designed to assign to each logical core
a private buffer pool in local memory to minimize remote memory access.
The configuration of packet buffer pools should take into account the underlying physical memory architecture in terms of DIMMS,
channels and ranks.
The application must ensure that appropriate parameters are given at memory pool creation time.
See :ref:`Mempool Library <Mempool_Library>`.

Design Principles
-----------------

The API and architecture of the Ethernet* PMDs are designed with the following guidelines in mind.

PMDs must help global policy-oriented decisions to be enforced at the upper application level.
Conversely, NIC PMD functions should not impede the benefits expected by upper-level global policies,
or worse prevent such policies from being applied.

For instance, both the receive and transmit functions of a PMD have a maximum number of packets/descriptors to poll.
This allows a run-to-completion processing stack to statically fix or
to dynamically adapt its overall behavior through different global loop policies, such as:

*   Receive, process immediately and transmit packets one at a time in a piecemeal fashion.

*   Receive as many packets as possible, then process all received packets, transmitting them immediately.

*   Receive a given maximum number of packets, process the received packets, accumulate them and finally send all accumulated packets to transmit.

To achieve optimal performance, overall software design choices and pure software optimization techniques must be considered and
balanced against available low-level hardware-based optimization features (CPU cache properties, bus speed, NIC PCI bandwidth, and so on).
The case of packet transmission is an example of this software/hardware tradeoff issue when optimizing burst-oriented network packet processing engines.
In the initial case, the PMD could export only an rte_eth_tx_one function to transmit one packet at a time on a given queue.
On top of that, one can easily build an rte_eth_tx_burst function that loops invoking the rte_eth_tx_one function to transmit several packets at a time.
However, an rte_eth_tx_burst function is effectively implemented by the PMD to minimize the driver-level transmit cost per packet through the following optimizations:

*   Share among multiple packets the un-amortized cost of invoking the rte_eth_tx_one function.

*   Enable the rte_eth_tx_burst function to take advantage of burst-oriented hardware features (prefetch data in cache, use of NIC head/tail registers)
    to minimize the number of CPU cycles per packet, for example by avoiding unnecessary read memory accesses to ring transmit descriptors,
    or by systematically using arrays of pointers that exactly fit cache line boundaries and sizes.

*   Apply burst-oriented software optimization techniques to remove operations that would otherwise be unavoidable, such as ring index wrap back management.

Burst-oriented functions are also introduced via the API for services that are intensively used by the PMD.
This applies in particular to buffer allocators used to populate NIC rings, which provide functions to allocate/free several buffers at a time.
For example, an mbuf_multiple_alloc function returning an array of pointers to rte_mbuf buffers which speeds up the receive poll function of the PMD when
replenishing multiple descriptors of the receive ring.

Logical Cores, Memory and NIC Queues Relationships
--------------------------------------------------

The DPDK supports NUMA allowing for better performance when a processor's logical cores and interfaces utilize its local memory.
Therefore, mbuf allocation associated with local PCIe* interfaces should be allocated from memory pools created in the local memory.
The buffers should, if possible, remain on the local processor to obtain the best performance results and RX and TX buffer descriptors
should be populated with mbufs allocated from a mempool allocated from local memory.

The run-to-completion model also performs better if packet or data manipulation is in local memory instead of a remote processors memory.
This is also true for the pipe-line model provided all logical cores used are located on the same processor.

Multiple logical cores should never share receive or transmit queues for interfaces since this would require global locks and hinder performance.

If the PMD is ``DEV_TX_OFFLOAD_MT_LOCKFREE`` capable, multiple threads can invoke ``rte_eth_tx_burst()``
concurrently on the same tx queue without SW lock. This PMD feature found in some NICs and useful in the following use cases:

*  Remove explicit spinlock in some applications where lcores are not mapped to Tx queues with 1:1 relation.

*  In the eventdev use case, avoid dedicating a separate TX core for transmitting and thus
   enables more scaling as all workers can send the packets.

See `Hardware Offload`_ for ``DEV_TX_OFFLOAD_MT_LOCKFREE`` capability probing details.

Device Identification and Configuration
---------------------------------------

Device Identification
~~~~~~~~~~~~~~~~~~~~~

Each NIC port is uniquely designated by its (bus/bridge, device, function) PCI
identifiers assigned by the PCI probing/enumeration function executed at DPDK initialization.
Based on their PCI identifier, NIC ports are assigned two other identifiers:

*   A port index used to designate the NIC port in all functions exported by the PMD API.

*   A port name used to designate the port in console messages, for administration or debugging purposes.
    For ease of use, the port name includes the port index.

Device Configuration
~~~~~~~~~~~~~~~~~~~~

The configuration of each NIC port includes the following operations:

*   Allocate PCI resources

*   Reset the hardware (issue a Global Reset) to a well-known default state

*   Set up the PHY and the link

*   Initialize statistics counters

The PMD API must also export functions to start/stop the all-multicast feature of a port and functions to set/unset the port in promiscuous mode.

Some hardware offload features must be individually configured at port initialization through specific configuration parameters.
This is the case for the Receive Side Scaling (RSS) and Data Center Bridging (DCB) features for example.

On-the-Fly Configuration
~~~~~~~~~~~~~~~~~~~~~~~~

All device features that can be started or stopped "on the fly" (that is, without stopping the device) do not require the PMD API to export dedicated functions for this purpose.

All that is required is the mapping address of the device PCI registers to implement the configuration of these features in specific functions outside of the drivers.

For this purpose,
the PMD API exports a function that provides all the information associated with a device that can be used to set up a given device feature outside of the driver.
This includes the PCI vendor identifier, the PCI device identifier, the mapping address of the PCI device registers, and the name of the driver.

The main advantage of this approach is that it gives complete freedom on the choice of the API used to configure, to start, and to stop such features.

As an example, refer to the configuration of the IEEE1588 feature for the Intel® 82576 Gigabit Ethernet Controller and
the Intel® 82599 10 Gigabit Ethernet Controller controllers in the testpmd application.

Other features such as the L3/L4 5-Tuple packet filtering feature of a port can be configured in the same way.
Ethernet* flow control (pause frame) can be configured on the individual port.
Refer to the testpmd source code for details.
Also, L4 (UDP/TCP/ SCTP) checksum offload by the NIC can be enabled for an individual packet as long as the packet mbuf is set up correctly. See `Hardware Offload`_ for details.

Configuration of Transmit Queues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each transmit queue is independently configured with the following information:

*   The number of descriptors of the transmit ring

*   The socket identifier used to identify the appropriate DMA memory zone from which to allocate the transmit ring in NUMA architectures

*   The values of the Prefetch, Host and Write-Back threshold registers of the transmit queue

*   The *minimum* transmit packets to free threshold (tx_free_thresh).
    When the number of descriptors used to transmit packets exceeds this threshold, the network adaptor should be checked to see if it has written back descriptors.
    A value of 0 can be passed during the TX queue configuration to indicate the default value should be used.
    The default value for tx_free_thresh is 32.
    This ensures that the PMD does not search for completed descriptors until at least 32 have been processed by the NIC for this queue.

*   The *minimum*  RS bit threshold. The minimum number of transmit descriptors to use before setting the Report Status (RS) bit in the transmit descriptor.
    Note that this parameter may only be valid for Intel 10 GbE network adapters.
    The RS bit is set on the last descriptor used to transmit a packet if the number of descriptors used since the last RS bit setting,
    up to the first descriptor used to transmit the packet, exceeds the transmit RS bit threshold (tx_rs_thresh).
    In short, this parameter controls which transmit descriptors are written back to host memory by the network adapter.
    A value of 0 can be passed during the TX queue configuration to indicate that the default value should be used.
    The default value for tx_rs_thresh is 32.
    This ensures that at least 32 descriptors are used before the network adapter writes back the most recently used descriptor.
    This saves upstream PCIe* bandwidth resulting from TX descriptor write-backs.
    It is important to note that the TX Write-back threshold (TX wthresh) should be set to 0 when tx_rs_thresh is greater than 1.
    Refer to the Intel® 82599 10 Gigabit Ethernet Controller Datasheet for more details.

The following constraints must be satisfied for tx_free_thresh and tx_rs_thresh:

*   tx_rs_thresh must be greater than 0.

*   tx_rs_thresh must be less than the size of the ring minus 2.

*   tx_rs_thresh must be less than or equal to tx_free_thresh.

*   tx_free_thresh must be greater than 0.

*   tx_free_thresh must be less than the size of the ring minus 3.

*   For optimal performance, TX wthresh should be set to 0 when tx_rs_thresh is greater than 1.

One descriptor in the TX ring is used as a sentinel to avoid a hardware race condition, hence the maximum threshold constraints.

.. note::

    When configuring for DCB operation, at port initialization, both the number of transmit queues and the number of receive queues must be set to 128.

Free Tx mbuf on Demand
~~~~~~~~~~~~~~~~~~~~~~

Many of the drivers do not release the mbuf back to the mempool, or local cache,
immediately after the packet has been transmitted.
Instead, they leave the mbuf in their Tx ring and
either perform a bulk release when the ``tx_rs_thresh`` has been crossed
or free the mbuf when a slot in the Tx ring is needed.

An application can request the driver to release used mbufs with the ``rte_eth_tx_done_cleanup()`` API.
This API requests the driver to release mbufs that are no longer in use,
independent of whether or not the ``tx_rs_thresh`` has been crossed.
There are two scenarios when an application may want the mbuf released immediately:

* When a given packet needs to be sent to multiple destination interfaces
  (either for Layer 2 flooding or Layer 3 multi-cast).
  One option is to make a copy of the packet or a copy of the header portion that needs to be manipulated.
  A second option is to transmit the packet and then poll the ``rte_eth_tx_done_cleanup()`` API
  until the reference count on the packet is decremented.
  Then the same packet can be transmitted to the next destination interface.
  The application is still responsible for managing any packet manipulations needed
  between the different destination interfaces, but a packet copy can be avoided.
  This API is independent of whether the packet was transmitted or dropped,
  only that the mbuf is no longer in use by the interface.

* Some applications are designed to make multiple runs, like a packet generator.
  For performance reasons and consistency between runs,
  the application may want to reset back to an initial state
  between each run, where all mbufs are returned to the mempool.
  In this case, it can call the ``rte_eth_tx_done_cleanup()`` API
  for each destination interface it has been using
  to request it to release of all its used mbufs.

To determine if a driver supports this API, check for the *Free Tx mbuf on demand* feature
in the *Network Interface Controller Drivers* document.

Hardware Offload
~~~~~~~~~~~~~~~~

Depending on driver capabilities advertised by
``rte_eth_dev_info_get()``, the PMD may support hardware offloading
feature like checksumming, TCP segmentation, VLAN insertion or
lockfree multithreaded TX burst on the same TX queue.

The support of these offload features implies the addition of dedicated
status bit(s) and value field(s) into the rte_mbuf data structure, along
with their appropriate handling by the receive/transmit functions
exported by each PMD. The list of flags and their precise meaning is
described in the mbuf API documentation and in the in :ref:`Mbuf Library
<Mbuf_Library>`, section "Meta Information".

Poll Mode Driver API
--------------------

Generalities
~~~~~~~~~~~~

By default, all functions exported by a PMD are lock-free functions that are assumed
not to be invoked in parallel on different logical cores to work on the same target object.
For instance, a PMD receive function cannot be invoked in parallel on two logical cores to poll the same RX queue of the same port.
Of course, this function can be invoked in parallel by different logical cores on different RX queues.
It is the responsibility of the upper-level application to enforce this rule.

If needed, parallel accesses by multiple logical cores to shared queues can be explicitly protected by dedicated inline lock-aware functions
built on top of their corresponding lock-free functions of the PMD API.

Generic Packet Representation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A packet is represented by an rte_mbuf structure, which is a generic metadata structure containing all necessary housekeeping information.
This includes fields and status bits corresponding to offload hardware features, such as checksum computation of IP headers or VLAN tags.

The rte_mbuf data structure includes specific fields to represent, in a generic way, the offload features provided by network controllers.
For an input packet, most fields of the rte_mbuf structure are filled in by the PMD receive function with the information contained in the receive descriptor.
Conversely, for output packets, most fields of rte_mbuf structures are used by the PMD transmit function to initialize transmit descriptors.

The mbuf structure is fully described in the :ref:`Mbuf Library <Mbuf_Library>` chapter.

Ethernet Device API
~~~~~~~~~~~~~~~~~~~

The Ethernet device API exported by the Ethernet PMDs is described in the *DPDK API Reference*.

Extended Statistics API
~~~~~~~~~~~~~~~~~~~~~~~

The extended statistics API allows a PMD to expose all statistics that are
available to it, including statistics that are unique to the device.
Each statistic has three properties ``name``, ``id`` and ``value``:

* ``name``: A human readable string formatted by the scheme detailed below.
* ``id``: An integer that represents only that statistic.
* ``value``: A unsigned 64-bit integer that is the value of the statistic.

Note that extended statistic identifiers are
driver-specific, and hence might not be the same for different ports.
The API consists of various ``rte_eth_xstats_*()`` functions, and allows an
application to be flexible in how it retrieves statistics.

Scheme for Human Readable Names
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A naming scheme exists for the strings exposed to clients of the API. This is
to allow scraping of the API for statistics of interest. The naming scheme uses
strings split by a single underscore ``_``. The scheme is as follows:

* direction
* detail 1
* detail 2
* detail n
* unit

Examples of common statistics xstats strings, formatted to comply to the scheme
proposed above:

* ``rx_bytes``
* ``rx_crc_errors``
* ``tx_multicast_packets``

The scheme, although quite simple, allows flexibility in presenting and reading
information from the statistic strings. The following example illustrates the
naming scheme:``rx_packets``. In this example, the string is split into two
components. The first component ``rx`` indicates that the statistic is
associated with the receive side of the NIC.  The second component ``packets``
indicates that the unit of measure is packets.

A more complicated example: ``tx_size_128_to_255_packets``. In this example,
``tx`` indicates transmission, ``size``  is the first detail, ``128`` etc are
more details, and ``packets`` indicates that this is a packet counter.

Some additions in the metadata scheme are as follows:

* If the first part does not match ``rx`` or ``tx``, the statistic does not
  have an affinity with either receive of transmit.

* If the first letter of the second part is ``q`` and this ``q`` is followed
  by a number, this statistic is part of a specific queue.

An example where queue numbers are used is as follows: ``tx_q7_bytes`` which
indicates this statistic applies to queue number 7, and represents the number
of transmitted bytes on that queue.

API Design
^^^^^^^^^^

The xstats API uses the ``name``, ``id``, and ``value`` to allow performant
lookup of specific statistics. Performant lookup means two things;

* No string comparisons with the ``name`` of the statistic in fast-path
* Allow requesting of only the statistics of interest

The API ensures these requirements are met by mapping the ``name`` of the
statistic to a unique ``id``, which is used as a key for lookup in the fast-path.
The API allows applications to request an array of ``id`` values, so that the
PMD only performs the required calculations. Expected usage is that the
application scans the ``name`` of each statistic, and caches the ``id``
if it has an interest in that statistic. On the fast-path, the integer can be used
to retrieve the actual ``value`` of the statistic that the ``id`` represents.

API Functions
^^^^^^^^^^^^^

The API is built out of a small number of functions, which can be used to
retrieve the number of statistics and the names, IDs and values of those
statistics.

* ``rte_eth_xstats_get_names_by_id()``: returns the names of the statistics. When given a
  ``NULL`` parameter the function returns the number of statistics that are available.

* ``rte_eth_xstats_get_id_by_name()``: Searches for the statistic ID that matches
  ``xstat_name``. If found, the ``id`` integer is set.

* ``rte_eth_xstats_get_by_id()``: Fills in an array of ``uint64_t`` values
  with matching the provided ``ids`` array. If the ``ids`` array is NULL, it
  returns all statistics that are available.


Application Usage
^^^^^^^^^^^^^^^^^

Imagine an application that wants to view the dropped packet count. If no
packets are dropped, the application does not read any other metrics for
performance reasons. If packets are dropped, the application has a particular
set of statistics that it requests. This "set" of statistics allows the app to
decide what next steps to perform. The following code-snippets show how the
xstats API can be used to achieve this goal.

First step is to get all statistics names and list them:

.. code-block:: c

    struct rte_eth_xstat_name *xstats_names;
    uint64_t *values;
    int len, i;

    /* Get number of stats */
    len = rte_eth_xstats_get_names_by_id(port_id, NULL, NULL, 0);
    if (len < 0) {
        printf("Cannot get xstats count\n");
        goto err;
    }

    xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * len);
    if (xstats_names == NULL) {
        printf("Cannot allocate memory for xstat names\n");
        goto err;
    }

    /* Retrieve xstats names, passing NULL for IDs to return all statistics */
    if (len != rte_eth_xstats_get_names_by_id(port_id, xstats_names, NULL, len)) {
        printf("Cannot get xstat names\n");
        goto err;
    }

    values = malloc(sizeof(values) * len);
    if (values == NULL) {
        printf("Cannot allocate memory for xstats\n");
        goto err;
    }

    /* Getting xstats values */
    if (len != rte_eth_xstats_get_by_id(port_id, NULL, values, len)) {
        printf("Cannot get xstat values\n");
        goto err;
    }

    /* Print all xstats names and values */
    for (i = 0; i < len; i++) {
        printf("%s: %"PRIu64"\n", xstats_names[i].name, values[i]);
    }

The application has access to the names of all of the statistics that the PMD
exposes. The application can decide which statistics are of interest, cache the
ids of those statistics by looking up the name as follows:

.. code-block:: c

    uint64_t id;
    uint64_t value;
    const char *xstat_name = "rx_errors";

    if(!rte_eth_xstats_get_id_by_name(port_id, xstat_name, &id)) {
        rte_eth_xstats_get_by_id(port_id, &id, &value, 1);
        printf("%s: %"PRIu64"\n", xstat_name, value);
    }
    else {
        printf("Cannot find xstats with a given name\n");
        goto err;
    }

The API provides flexibility to the application so that it can look up multiple
statistics using an array containing multiple ``id`` numbers. This reduces the
function call overhead of retrieving statistics, and makes lookup of multiple
statistics simpler for the application.

.. code-block:: c

    #define APP_NUM_STATS 4
    /* application cached these ids previously; see above */
    uint64_t ids_array[APP_NUM_STATS] = {3,4,7,21};
    uint64_t value_array[APP_NUM_STATS];

    /* Getting multiple xstats values from array of IDs */
    rte_eth_xstats_get_by_id(port_id, ids_array, value_array, APP_NUM_STATS);

    uint32_t i;
    for(i = 0; i < APP_NUM_STATS; i++) {
        printf("%d: %"PRIu64"\n", ids_array[i], value_array[i]);
    }


This array lookup API for xstats allows the application create multiple
"groups" of statistics, and look up the values of those IDs using a single API
call. As an end result, the application is able to achieve its goal of
monitoring a single statistic ("rx_errors" in this case), and if that shows
packets being dropped, it can easily retrieve a "set" of statistics using the
IDs array parameter to ``rte_eth_xstats_get_by_id`` function.
