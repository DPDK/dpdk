..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

Poll Mode Driver
================

The Data Plane Development Kit (DPDK) supports a wide range of Ethernet speeds,
from 10 Megabits to 400 Gigabits, depending on hardware capability.

DPDK Poll Mode Drivers (PMDs) are high-performance, optimized drivers for various
network interface cards that bypass the traditional kernel network stack to reduce
latency and improve throughput. They access Rx and Tx descriptors directly in a polling
mode without relying on interrupts (except for Link Status Change notifications), enabling
efficient packet reception and transmission in user-space applications.

This section outlines the requirements of Ethernet PMDs, their design principles,
and presents a high-level architecture along with a generic external API.

Requirements and Assumptions
----------------------------

The DPDK environment for packet processing applications supports two models: run-to-completion and pipeline.

*   In the *run-to-completion* model, a specific port's Rx descriptor ring is polled for packets through an API.
    The application then processes packets on the same core and transmits them via the port's Tx descriptor ring using another API.

*   In the *pipeline* model, one core polls the Rx descriptor ring(s) of one or more ports via an API.
    The application then passes received packets to another core via a ring for further processing,
    which may include transmission through the Tx descriptor ring using an API.

In a synchronous run-to-completion model, a logical core (lcore)
assigned to DPDK executes a packet processing loop that includes the following steps:

*   Retrieve input packets using the PMD receive API

*   Process each received packet individually, up to its forwarding

*   Transmit output packets using the PMD transmit API

In contrast, the asynchronous pipeline model assigns some logical cores to retrieve packets
and others to process them. The application exchanges packets between cores via rings.

The packet retrieval loop includes:

*   Retrieve input packets through the PMD receive API

*   Provide received packets to processing lcores through packet queues

The packet processing loop includes:

*   Dequeue received packets from the packet queue

*   Process packets, including retransmission if forwarded

To minimize interrupt-related overhead, the execution environment should avoid asynchronous
notification mechanisms. When asynchronous communication is required, implement it
using rings where possible. Minimizing lock contention is critical in multi-core environments.
To support this, PMDs use per-core private resources whenever possible.
For example, if a PMD does not support ``RTE_ETH_TX_OFFLOAD_MT_LOCKFREE``, it maintains a separate
transmit queue per core and per port. Similarly, each receive queue is assigned to and polled by a single lcore.

To support Non-Uniform Memory Access (NUMA), the memory management design assigns each logical
core a private buffer pool in local memory to reduce remote memory access. Configuration of packet
buffer pools should consider the underlying physical memory layout, such as DIMMs, channels, and ranks.
The application must set proper parameters during memory pool creation.
See :doc:`../mempool_lib`.

Design Principles
-----------------

The API and architecture of the Ethernet PMDs follow these design principles:

PMDs should support the enforcement of global, policy-driven decisions at the upper application level.
At the same time, NIC PMD functions must not hinder the performance gains expected by these higher-level policies,
or worse, prevent them from being implemented.

For example, both the receive and transmit functions of a PMD define a maximum number of packets to poll.
This enables a run-to-completion processing stack to either statically configure or dynamically adjust its
behavior according to different global loop strategies, such as:

*   Receiving, processing, and transmitting packets one at a time in a piecemeal fashion

*   Receiving as many packets as possible, then processing and transmitting them all immediately

*   Receiving a set number of packets, processing them, and batching them for transmission at once

To maximize performance, developers must consider overall software architecture and optimization techniques
alongside available low-level hardware optimizations (e.g., CPU cache behavior, bus speed, and NIC PCI bandwidth).

Packet transmission in burst-oriented network engines illustrates this software/hardware tradeoff.
A PMD could expose only the ``rte_eth_tx_one`` function to transmit a single packet at a time on a given queue.
While it is possible to build an ``rte_eth_tx_burst`` function by repeatedly calling ``rte_eth_tx_one``,
most PMDs implement ``rte_eth_tx_burst`` directly to reduce per-packet transmission overhead.

This implementation includes several key optimizations:

*   Sharing the fixed cost of invoking ``rte_eth_tx_one`` across multiple packets

*   Leveraging burst-oriented hardware features (e.g., data prefetching, NIC head/tail registers, vector extensions)
    to reduce CPU cycles per packet, including minimizing unnecessary memory accesses and aligning pointer arrays
    with cache line boundaries and sizes

*   Applying software-level burst optimizations to eliminate otherwise unavoidable overheads, such as ring index wrap-around handling

The API also introduces burst-oriented functions for PMD-intensive services, such as buffer allocation.
For instance, buffer allocators used to populate NIC rings often support functions that allocate or free multiple buffers in a single call.
An example is ``rte_pktmbuf_alloc_bulk``, which returns an array of rte_mbuf pointers, significantly improving PMD performance
when replenishing multiple descriptors in the receive ring.

Logical Cores, Memory and NIC Queues Relationships
--------------------------------------------------

DPDK supports NUMA, which improves performance when a processor's logical cores and network interfaces
use memory local to that processor. To maximize this benefit, allocate mbufs associated with local PCIe* interfaces
from memory pools located in the same NUMA node.

Ideally, keep these buffers on the local processor to achieve optimal performance. Populate Rx and Tx buffer
descriptors with mbufs from mempools created in local memory.

The run-to-completion model also benefits from performing packet data operations in local memory,
rather than accessing remote memory across NUMA nodes.
The same applies to the pipeline model, provided all logical cores involved reside on the same processor.

Never share receive and transmit queues between multiple logical cores, as doing so requires
global locks and severely impacts performance.

If the PMD supports the ``RTE_ETH_TX_OFFLOAD_MT_LOCKFREE`` offload,
multiple threads can call ``rte_eth_tx_burst()`` concurrently on the same Tx queue without a software lock.
This capability, available in some NICs, proves advantageous in these scenarios:

*   Eliminating explicit spinlocks in applications where Tx queues do not map 1:1 to logical cores

*   In eventdev-based workloads, allowing all worker threads to transmit packets, removing the need for a dedicated
    Tx core and enabling greater scalability

See `Hardware Offload`_ for ``RTE_ETH_TX_OFFLOAD_MT_LOCKFREE`` capability probing details.

Device Identification, Ownership and Configuration
--------------------------------------------------

Device Identification
~~~~~~~~~~~~~~~~~~~~~

The PCI probing/enumeration function executed at DPDK initialization assigns each NIC port a unique PCI
identifier (bus/bridge, device, function). Based on this PCI identifier, DPDK assigns each NIC port two additional identifiers:

*   A port index used to designate the NIC port in all functions exported by the PMD API

*   A port name used to designate the port in console messages, for administration or debugging purposes.
    For ease of use, the port name includes the port index.

Port Ownership
~~~~~~~~~~~~~~

A single DPDK entity (application, library, PMD, process, etc.) can own Ethernet device ports.
The ethdev APIs control the ownership mechanism and allow DPDK entities to set, remove, or get a port owner.
This prevents different entities from managing the same Ethernet ports.

.. note::

    The DPDK entity must set port ownership before using the port and manage usage synchronization between different threads or processes.

Set port ownership early, for instance during the probing notification ``RTE_ETH_EVENT_NEW``.

Device Configuration
~~~~~~~~~~~~~~~~~~~~

Configuring each NIC port includes the following operations:

*   Allocate PCI resources

*   Reset the hardware to a well-known default state (issue a Global Reset)

*   Set up the PHY and the link

*   Initialize statistics counters

The PMD API must also export functions to start/stop the all-multicast feature of a port and functions to set/unset promiscuous mode.

Some hardware offload features require individual configuration at port initialization through specific parameters.
This includes Receive Side Scaling (RSS) and Data Center Bridging (DCB) features.

On-the-Fly Configuration
~~~~~~~~~~~~~~~~~~~~~~~~

Device features that can start or stop "on the fly" (without stopping the device) do not require the PMD API to export dedicated functions.

Implementing the configuration of these features in specific functions outside of the drivers requires only the mapping address of the device PCI registers.

For this purpose,
the PMD API exports a function that provides all device information needed to set up a given feature outside of the driver.
This includes the PCI vendor identifier, the PCI device identifier, the mapping address of the PCI device registers, and the driver name.

The main advantage of this approach is complete freedom in choosing the API to configure, start, and stop such features.

As an example, refer to the configuration of the IEEE1588 feature for the Intel® 82576 Gigabit Ethernet Controller and
the Intel® 82599 10 Gigabit Ethernet Controller in the testpmd application.

Configure other features such as the L3/L4 5-Tuple packet filtering feature of a port in the same way.
Configure Ethernet* flow control (pause frame) on an individual port.
Refer to the testpmd source code for details.
Also, enable L4 (UDP/TCP/SCTP) checksum offload by the NIC for an individual packet by setting up the packet mbuf correctly. See `Hardware Offload`_ for details.

Configuration of Transmit Queues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure each transmit queue independently with the following information:

*   The number of descriptors of the transmit ring

*   The socket identifier used to identify the appropriate DMA memory zone for allocating the transmit ring in NUMA architectures

*   The values of the Prefetch, Host and Write-Back threshold registers of the transmit queue

*   The *minimum* transmit packets to free threshold (tx_free_thresh).
    When the number of descriptors used to transmit packets exceeds this threshold, check the network adapter to see if it has written back descriptors.
    Pass a value of 0 during Tx queue configuration to use the default value.
    The default value for tx_free_thresh is 32.
    This ensures the PMD does not search for completed descriptors until the NIC has processed at least 32 for this queue.

*   The *minimum* RS bit threshold. The minimum number of transmit descriptors to use before setting the Report Status (RS) bit in the transmit descriptor.
    Note that this parameter may only be valid for Intel 10 GbE network adapters.
    Set the RS bit on the last descriptor used to transmit a packet if the number of descriptors used since the last RS bit setting,
    up to the first descriptor used to transmit the packet, exceeds the transmit RS bit threshold (tx_rs_thresh).
    In short, this parameter controls which transmit descriptors the network adapter writes back to host memory.
    Pass a value of 0 during Tx queue configuration to use the default value.
    The default value for tx_rs_thresh is 32.
    This ensures the PMD uses at least 32 descriptors before the network adapter writes back the most recently used descriptor.
    This saves upstream PCIe* bandwidth that would be used for Tx descriptor write-backs.
    Set the Tx Write-back threshold (Tx wthresh) to 0 when tx_rs_thresh is greater than 1.
    Refer to the Intel® 82599 10 Gigabit Ethernet Controller Datasheet for more details.

The following constraints must be satisfied for tx_free_thresh and tx_rs_thresh:

*   tx_rs_thresh must be greater than 0.

*   tx_rs_thresh must be less than the size of the ring minus 2.

*   tx_rs_thresh must be less than or equal to tx_free_thresh.

*   tx_free_thresh must be greater than 0.

*   tx_free_thresh must be less than the size of the ring minus 3.

*   For optimal performance, set Tx wthresh to 0 when tx_rs_thresh is greater than 1.

One descriptor in the Tx ring serves as a sentinel to avoid a hardware race condition, hence the maximum threshold constraints.

.. note::

    When configuring for DCB operation at port initialization, set both the number of transmit queues and the number of receive queues to 128.

Free Tx mbuf on Demand
~~~~~~~~~~~~~~~~~~~~~~

Many drivers do not release the mbuf back to the mempool or local cache immediately after packet transmission.
Instead, they leave the mbuf in their Tx ring and
either perform a bulk release when the ``tx_rs_thresh`` has been crossed
or free the mbuf when a slot in the Tx ring is needed.

An application can request the driver to release used mbufs with the ``rte_eth_tx_done_cleanup()`` API.
This API requests the driver to release mbufs no longer in use,
independent of whether the ``tx_rs_thresh`` has been crossed.
Two scenarios exist where an application may want the mbuf released immediately:

* When a given packet needs to be sent to multiple destination interfaces
  (either for Layer 2 flooding or Layer 3 multi-cast).
  One option is to copy the packet or the header portion that needs manipulation.
  A second option is to transmit the packet and then poll the ``rte_eth_tx_done_cleanup()`` API
  until the reference count on the packet decrements.
  Then, transmit the same packet to the next destination interface.
  The application remains responsible for managing any packet manipulations needed
  between the different destination interfaces, but avoids a packet copy.
  This API operates independently of whether the interface transmitted or dropped the packet,
  only that the mbuf is no longer in use by the interface.

* Some applications make multiple runs, like a packet generator.
  For performance reasons and consistency between runs,
  the application may want to reset back to an initial state
  between each run, where all mbufs are returned to the mempool.
  In this case, call the ``rte_eth_tx_done_cleanup()`` API
  for each destination interface used
  to request it to release all used mbufs.

To determine if a driver supports this API, check for the *Free Tx mbuf on demand* feature
in the *Network Interface Controller Drivers* document.

Hardware Offload
~~~~~~~~~~~~~~~~

Depending on driver capabilities advertised by
``rte_eth_dev_info_get()``, the PMD may support hardware offloading
features like checksumming, TCP segmentation, VLAN insertion, or
lockfree multithreaded Tx burst on the same Tx queue.

Supporting these offload features requires adding dedicated
status bit(s) and value field(s) to the rte_mbuf data structure, along
with appropriate handling by the receive/transmit functions
exported by each PMD. The mbuf API documentation and the :ref:`mbuf_meta` chapter
describe the list of flags and their precise meanings.

Per-Port and Per-Queue Offloads
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the DPDK offload API, offloads divide into per-port and per-queue offloads as follows:

* A per-queue offload can be enabled on one queue and disabled on another queue simultaneously.
* A pure per-port offload is supported by a device but not as a per-queue type.
* A pure per-port offload cannot be enabled on one queue and disabled on another queue simultaneously.
* A pure per-port offload must be enabled or disabled on all queues simultaneously.
* An offload is either per-queue or pure per-port type; it cannot be both types on the same device.
* Port capabilities = per-queue capabilities + pure per-port capabilities.
* Any supported offload can be enabled on all queues.

Query the different offload capabilities using ``rte_eth_dev_info_get()``.
The ``dev_info->[rt]x_queue_offload_capa`` returned from ``rte_eth_dev_info_get()`` includes all per-queue offloading capabilities.
The ``dev_info->[rt]x_offload_capa`` returned from ``rte_eth_dev_info_get()`` includes all pure per-port and per-queue offloading capabilities.
Supported offloads can be either per-port or per-queue.

Enable offloads using the existing ``RTE_ETH_TX_OFFLOAD_*`` or ``RTE_ETH_RX_OFFLOAD_*`` flags.
Any offload requested by an application must be within the device capabilities.
Any offload is disabled by default if it is not set in the parameter
``dev_conf->[rt]xmode.offloads`` to ``rte_eth_dev_configure()`` and
``[rt]x_conf->offloads`` to ``rte_eth_[rt]x_queue_setup()``.

If an application enables any offload in ``rte_eth_dev_configure()``,
it is enabled on all queues regardless of whether it is per-queue or
per-port type and regardless of whether it is set or cleared in
``[rt]x_conf->offloads`` to ``rte_eth_[rt]x_queue_setup()``.

If a per-queue offload has not been enabled in ``rte_eth_dev_configure()``,
it can be enabled or disabled in ``rte_eth_[rt]x_queue_setup()`` for an individual queue.
A newly added offload in ``[rt]x_conf->offloads`` to ``rte_eth_[rt]x_queue_setup()`` input by the application
is one that has not been enabled in ``rte_eth_dev_configure()`` and is requested to be enabled
in ``rte_eth_[rt]x_queue_setup()``. It must be per-queue type; otherwise an error log triggers.

Poll Mode Driver API
--------------------

Generalities
~~~~~~~~~~~~

By default, all functions exported by a PMD are lock-free functions assumed
not to be invoked in parallel on different logical cores working on the same target object.
For instance, a PMD receive function cannot be invoked in parallel on two logical cores polling the same Rx queue of the same port.
This function can be invoked in parallel by different logical cores on different Rx queues.
The upper-level application must enforce this rule.

If needed, explicitly protect parallel accesses by multiple logical cores to shared queues using dedicated inline lock-aware functions
built on top of their corresponding lock-free functions of the PMD API.

Generic Packet Representation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An rte_mbuf structure represents a packet. This generic metadata structure contains all necessary housekeeping information,
including fields and status bits corresponding to offload hardware features, such as checksum computation of IP headers or VLAN tags.

The rte_mbuf data structure includes specific fields to represent, in a generic way, the offload features provided by network controllers.
For an input packet, the PMD receive function fills in most fields of the rte_mbuf structure with information contained in the receive descriptor.
Conversely, for output packets, the PMD transmit function uses most fields of rte_mbuf structures to initialize transmit descriptors.

See :doc:`../mbuf_lib` chapter for more details.

Ethernet Device API
~~~~~~~~~~~~~~~~~~~

The *DPDK API Reference* describes the Ethernet device API exported by the Ethernet PMDs.

.. _ethernet_device_standard_device_arguments:

Ethernet Device Standard Device Arguments
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Standard Ethernet device arguments provide a set of commonly used arguments/
parameters applicable to all Ethernet devices. Use these arguments/parameters to
specify specific devices and pass common configuration parameters to those ports.

* Use ``representor`` for a device that supports creating representor ports.
  This argument allows the user to specify which switch ports to enable port
  representors for::

   -a DBDF,representor=vf0
   -a DBDF,representor=vf[0,4,6,9]
   -a DBDF,representor=vf[0-31]
   -a DBDF,representor=vf[0,2-4,7,9-11]

  These examples attach VF representors relative to DBDF.
  The VF IDs can be a list, a range, or a mix.
  SF representors follow the same syntax::

   -a DBDF,representor=sf0
   -a DBDF,representor=sf[1,3,5]
   -a DBDF,representor=sf[0-1023]
   -a DBDF,representor=sf[0,2-4,7,9-11]

  If multiple PFs are associated with the same PCI device,
  use the PF ID to distinguish between representors relative to different PFs::

   -a DBDF,representor=pf1vf0
   -a DBDF,representor=pf[0-1]vf0

  The example above attaches 4 representors pf0vf0, pf1vf0, pf0, and pf1.
  If only VF representors are required, enclose the PF part in parentheses::

   -a DBDF,representor=(pf[0-1])vf0

  The example above attaches 2 representors pf0vf0 and pf1vf0.

  Enclose the list of representors for the same PCI device in square brackets::

   -a DBDF,representor=[pf[0-1],pf2vf[0-2],pf3[3,5-8]]

  Note: PMDs may have additional extensions for the representor parameter. Consult
  the relevant PMD documentation for supported devargs.

Extended Statistics API
~~~~~~~~~~~~~~~~~~~~~~~

The extended statistics API allows a PMD to expose all available statistics,
including statistics unique to the device.
Each statistic has three properties: ``name``, ``id``, and ``value``:

* ``name``: A human-readable string formatted by the scheme detailed below.
* ``id``: An integer that represents only that statistic.
* ``value``: An unsigned 64-bit integer that is the value of the statistic.

Note that extended statistic identifiers are driver-specific,
and therefore might not be the same for different ports.
The API consists of various ``rte_eth_xstats_*()`` functions and provides
applications flexibility in how they retrieve statistics.

Scheme for Human Readable Names
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A naming scheme governs the strings exposed to clients of the API. This scheme
allows scraping of the API for statistics of interest. The naming scheme uses
strings split by a single underscore ``_``. The scheme is as follows:

* direction
* detail 1
* detail 2
* detail n
* unit

Examples of common statistics xstats strings, formatted to comply with the scheme
proposed above:

* ``rx_bytes``
* ``rx_crc_errors``
* ``tx_multicast_packets``

The scheme, although simple, provides flexibility in presenting and reading
information from the statistic strings. The following example illustrates the
naming scheme: ``rx_packets``. In this example, the string splits into two
components. The first component ``rx`` indicates that the statistic
is associated with the receive side of the NIC. The second component ``packets``
indicates that the unit of measure is packets.

A more complicated example: ``tx_size_128_to_255_packets``. In this example,
``tx`` indicates transmission, ``size`` is the first detail, ``128`` etc. are
more details, and ``packets`` indicates that this is a packet counter.

Some additions in the metadata scheme are as follows:

* If the first part does not match ``rx`` or ``tx``, the statistic does not
  have an affinity with either receive or transmit.

* If the first letter of the second part is ``q`` and this ``q`` is followed
  by a number, this statistic is part of a specific queue.

An example where queue numbers are used: ``tx_q7_bytes`` indicates this statistic applies to queue number 7 and represents the number
of transmitted bytes on that queue.

API Design
^^^^^^^^^^

The xstats API uses ``name``, ``id``, and ``value`` to allow performant
lookup of specific statistics. Performant lookup means two things:

* No string comparisons with the ``name`` of the statistic in the fast path
* Allow requesting only the statistics of interest

The API meets these requirements by mapping the ``name`` of the
statistic to a unique ``id``, which serves as a key for lookup in the fast path.
The API allows applications to request an array of ``id`` values, so the
PMD only performs the required calculations. The expected usage is that the
application scans the ``name`` of each statistic and caches the ``id``
if it has an interest in that statistic. On the fast path, the integer can be used
to retrieve the actual ``value`` of the statistic that the ``id`` represents.

API Functions
^^^^^^^^^^^^^

The API is built from a small number of functions, which retrieve the number of statistics
and the names, IDs, and values of those statistics.

* ``rte_eth_xstats_get_names_by_id()``: Returns the names of the statistics. When given a
  ``NULL`` parameter, the function returns the number of available statistics.

* ``rte_eth_xstats_get_id_by_name()``: Searches for the statistic ID that matches
  ``xstat_name``. If found, sets the ``id`` integer.

* ``rte_eth_xstats_get_by_id()``: Fills in an array of ``uint64_t`` values
  matching the provided ``ids`` array. If the ``ids`` array is NULL, it
  returns all available statistics.


Application Usage
^^^^^^^^^^^^^^^^^

Imagine an application that wants to view the dropped packet count. If no
packets are dropped, the application does not read any other metrics for
performance reasons. If packets are dropped, the application has a particular
set of statistics that it requests. This "set" of statistics allows the application to
decide what next steps to perform. The following code snippets show how the
xstats API achieves this goal.

The first step is to get all statistics names and list them:

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

The application has access to the names of all statistics that the PMD
exposes. The application can decide which statistics are of interest and cache the
IDs of those statistics by looking up the name as follows:

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

The API allows the application to look up multiple statistics using an array containing multiple ``id`` numbers.
This reduces function call overhead when retrieving statistics and simplifies looking up multiple statistics.

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


This array lookup API for xstats allows the application to create multiple
"groups" of statistics and look up the values of those IDs using a single API
call. As a result, the application achieves its goal of
monitoring a single statistic (in this case, "rx_errors"). If that shows
packets being dropped, it can easily retrieve a "set" of statistics using the
IDs array parameter to the ``rte_eth_xstats_get_by_id`` function.

NIC Reset API
~~~~~~~~~~~~~

.. code-block:: c

    int rte_eth_dev_reset(uint16_t port_id);

Sometimes a port must be reset passively. For example, when a PF is
reset, the application should also reset all its VFs to maintain consistency
with the PF. A DPDK application can also call this function
to trigger a port reset. Normally, a DPDK application invokes this
function when it detects an RTE_ETH_EVENT_INTR_RESET event.

The PMD triggers RTE_ETH_EVENT_INTR_RESET events.
The application should register a callback function to handle these
events. When a PMD needs to trigger a reset, it triggers an
RTE_ETH_EVENT_INTR_RESET event. On receiving an RTE_ETH_EVENT_INTR_RESET
event, applications should: stop working queues, stop
calling Rx and Tx functions, and then call rte_eth_dev_reset(). For
thread safety, call all these operations from the same thread.

For example, when a PF is reset, it sends a message to notify VFs of
this event and also triggers an interrupt to VFs. Then, in the interrupt
service routine, the VFs detect this notification message and call
rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_RESET, NULL).
This means that a PF reset triggers an RTE_ETH_EVENT_INTR_RESET
event within VFs. The function rte_eth_dev_callback_process()
calls the registered callback function. The callback function can trigger
the application to handle all operations the VF reset requires, including
stopping Rx/Tx queues and calling rte_eth_dev_reset().

The rte_eth_dev_reset() function is a generic function that only performs hardware reset operations by calling dev_uninit() and
dev_init(). It does not handle synchronization; the application handles that.

The PMD should not call rte_eth_dev_reset(). The PMD can trigger
the application to handle the reset event. The application must
handle all synchronization before calling rte_eth_dev_reset().

The above error handling mode is known as ``RTE_ETH_ERROR_HANDLE_MODE_PASSIVE``.

Proactive Error Handling Mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This mode is known as ``RTE_ETH_ERROR_HANDLE_MODE_PROACTIVE``, which
differs from PASSIVE mode where the application invokes recovery.
In PROACTIVE mode, the PMD automatically recovers from errors,
and the application requires only minimal handling.

During error detection and automatic recovery,
the PMD sets the data path pointers to dummy functions
(which prevent crashes)
and ensures control path operations fail with return code ``-EBUSY``.

Because the PMD recovers automatically,
the application only senses that the data flow is disconnected for a while
and that the control API returns an error during this period.

To sense error occurrence and recovery,
as well as to restore additional configuration,
three events are available:

``RTE_ETH_EVENT_ERR_RECOVERING``
   Notifies the application that an error is detected
   and recovery is beginning.
   Upon receiving the event, the application should not invoke
   any control path function until receiving the
   ``RTE_ETH_EVENT_RECOVERY_SUCCESS`` or ``RTE_ETH_EVENT_RECOVERY_FAILED`` event.

.. note::

   Before the PMD reports the recovery result,
   it may report the ``RTE_ETH_EVENT_ERR_RECOVERING`` event again
   because a larger error may occur during recovery.

``RTE_ETH_EVENT_RECOVERY_SUCCESS``
   Notifies the application that recovery from the error was successful.
   The PMD has reconfigured the port,
   and the effect is the same as a restart operation.

``RTE_ETH_EVENT_RECOVERY_FAILED``
   Notifies the application that recovery from the error failed.
   The port should not be usable anymore.
   The application should close the port.

Query the error handling mode supported by the PMD using ``rte_eth_dev_info_get()``.
