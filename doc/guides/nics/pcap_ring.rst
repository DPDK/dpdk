..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

Libpcap and Ring Based Poll Mode Drivers
========================================

In addition to Poll Mode Drivers (PMDs) for physical and virtual hardware,
the DPDK also includes pure-software PMDs, two of these drivers are:

*   A libpcap -based PMD (librte_pmd_pcap) that reads and writes packets using libpcap,
    - both from files on disk, as well as from physical NIC devices using standard Linux kernel drivers.

*   A ring-based PMD (librte_pmd_ring) that allows a set of software FIFOs (that is, rte_ring)
    to be accessed using the PMD APIs, as though they were physical NICs.

.. note::

    The libpcap -based PMD is disabled by default in the build configuration files,
    owing to an external dependency on the libpcap development files which must be installed on the board.
    Once the libpcap development files are installed,
    the library can be enabled by setting CONFIG_RTE_LIBRTE_PMD_PCAP=y and recompiling the DPDK.

Using the Drivers from the EAL Command Line
-------------------------------------------

For ease of use, the DPDK EAL also has been extended to allow pseudo-Ethernet devices,
using one or more of these drivers,
to be created at application startup time during EAL initialization.

To do so, the --vdev= parameter must be passed to the EAL.
This takes take options to allow ring and pcap-based Ethernet to be allocated and used transparently by the application.
This can be used, for example, for testing on a virtual machine where there are no Ethernet ports.

Libpcap-based PMD
~~~~~~~~~~~~~~~~~

Pcap-based devices can be created using the virtual device --vdev option.
The device name must start with the net_pcap prefix followed by numbers or letters.
The name is unique for each device. Each device can have multiple stream options and multiple devices can be used.
Multiple device definitions can be arranged using multiple --vdev.
Device name and stream options must be separated by commas as shown below:

.. code-block:: console

   $RTE_TARGET/app/testpmd -l 0-3 -n 4 \
       --vdev 'net_pcap0,stream_opt0=..,stream_opt1=..' \
       --vdev='net_pcap1,stream_opt0=..'

Device Streams
^^^^^^^^^^^^^^

Multiple ways of stream definitions can be assessed and combined as long as the following two rules are respected:

*   A device is provided with two different streams - reception and transmission.

*   A device is provided with one network interface name used for reading and writing packets.

The different stream types are:

*   rx_pcap: Defines a reception stream based on a pcap file.
    The driver reads each packet within the given pcap file as if it was receiving it from the wire.
    The value is a path to a valid pcap file.

        rx_pcap=/path/to/file.pcap

*   tx_pcap: Defines a transmission stream based on a pcap file.
    The driver writes each received packet to the given pcap file.
    The value is a path to a pcap file.
    The file is overwritten if it already exists and it is created if it does not.

        tx_pcap=/path/to/file.pcap

*   rx_iface: Defines a reception stream based on a network interface name.
    The driver reads packets from the given interface using the Linux kernel driver for that interface.
    The driver captures both the incoming and outgoing packets on that interface.
    The value is an interface name.

        rx_iface=eth0

*   rx_iface_in: Defines a reception stream based on a network interface name.
    The driver reads packets from the given interface using the Linux kernel driver for that interface.
    The driver captures only the incoming packets on that interface.
    The value is an interface name.

        rx_iface_in=eth0

*   tx_iface: Defines a transmission stream based on a network interface name.
    The driver sends packets to the given interface using the Linux kernel driver for that interface.
    The value is an interface name.

        tx_iface=eth0

*   iface: Defines a device mapping a network interface.
    The driver both reads and writes packets from and to the given interface.
    The value is an interface name.

        iface=eth0

Runtime Config Options
^^^^^^^^^^^^^^^^^^^^^^

- Use PCAP interface physical MAC

 In case ``iface=`` configuration is set, user may want to use the selected interface's physical MAC
 address. This can be done with a ``devarg`` ``phy_mac``, for example::

   --vdev 'net_pcap0,iface=eth0,phy_mac=1'

- Use the RX PCAP file to infinitely receive packets

 In case ``rx_pcap=`` configuration is set, user may want to use the selected PCAP file for rudimental
 performance testing. This can be done with a ``devarg`` ``infinite_rx``, for example::

   --vdev 'net_pcap0,rx_pcap=file_rx.pcap,infinite_rx=1'

 When this mode is used, it is recommended to drop all packets on transmit by not providing a tx_pcap or tx_iface.

 This option is device wide, so all queues on a device will either have this enabled or disabled.
 This option should only be provided once per device.

- Drop all packets on transmit

 The user may want to drop all packets on tx for a device. This can be done by not providing a tx_pcap or tx_iface, for example::

   --vdev 'net_pcap0,rx_pcap=file_rx.pcap'

 In this case, one tx drop queue is created for each rxq on that device.

 - Receive no packets on Rx

 The user may want to run without receiving any packets on Rx. This can be done by not providing a rx_pcap or rx_iface, for example::

   --vdev 'net_pcap0,tx_pcap=file_tx.pcap'

In this case, one dummy rx queue is created for each tx queue argument passed

Examples of Usage
^^^^^^^^^^^^^^^^^

Read packets from one pcap file and write them to another:

.. code-block:: console

    $RTE_TARGET/app/testpmd -l 0-3 -n 4 \
        --vdev 'net_pcap0,rx_pcap=file_rx.pcap,tx_pcap=file_tx.pcap' \
        -- --port-topology=chained

Read packets from a network interface and write them to a pcap file:

.. code-block:: console

    $RTE_TARGET/app/testpmd -l 0-3 -n 4 \
        --vdev 'net_pcap0,rx_iface=eth0,tx_pcap=file_tx.pcap' \
        -- --port-topology=chained

Read packets from a pcap file and write them to a network interface:

.. code-block:: console

    $RTE_TARGET/app/testpmd -l 0-3 -n 4 \
        --vdev 'net_pcap0,rx_pcap=file_rx.pcap,tx_iface=eth1' \
        -- --port-topology=chained

Forward packets through two network interfaces:

.. code-block:: console

    $RTE_TARGET/app/testpmd -l 0-3 -n 4 \
        --vdev 'net_pcap0,iface=eth0' --vdev='net_pcap1;iface=eth1'

Enable 2 tx queues on a network interface:

.. code-block:: console

    $RTE_TARGET/app/testpmd -l 0-3 -n 4 \
        --vdev 'net_pcap0,rx_iface=eth1,tx_iface=eth1,tx_iface=eth1' \
        -- --txq 2

Read only incoming packets from a network interface and write them back to the same network interface:

.. code-block:: console

    $RTE_TARGET/app/testpmd -l 0-3 -n 4 \
        --vdev 'net_pcap0,rx_iface_in=eth1,tx_iface=eth1'

Using libpcap-based PMD with the testpmd Application
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

One of the first things that testpmd does before starting to forward packets is to flush the RX streams
by reading the first 512 packets on every RX stream and discarding them.
When using a libpcap-based PMD this behavior can be turned off using the following command line option:

.. code-block:: console

    --no-flush-rx

It is also available in the runtime command line:

.. code-block:: console

    set flush_rx on/off

It is useful for the case where the rx_pcap is being used and no packets are meant to be discarded.
Otherwise, the first 512 packets from the input pcap file will be discarded by the RX flushing operation.

.. code-block:: console

    $RTE_TARGET/app/testpmd -l 0-3 -n 4 \
        --vdev 'net_pcap0,rx_pcap=file_rx.pcap,tx_pcap=file_tx.pcap' \
        -- --port-topology=chained --no-flush-rx

.. note::

   The network interface provided to the PMD should be up. The PMD will return
   an error if interface is down, and the PMD itself won't change the status
   of the external network interface.


Rings-based PMD
~~~~~~~~~~~~~~~

To run a DPDK application on a machine without any Ethernet devices, a pair of ring-based rte_ethdevs can be used as below.
The device names passed to the --vdev option must start with net_ring and take no additional parameters.
Multiple devices may be specified, separated by commas.

.. code-block:: console

    ./testpmd -l 1-3 -n 4 --vdev=net_ring0 --vdev=net_ring1 -- -i
    EAL: Detected lcore 1 as core 1 on socket 0
    ...

    Interactive-mode selected
    Configuring Port 0 (socket 0)
    Configuring Port 1 (socket 0)
    Checking link statuses...
    Port 0 Link Up - speed 10000 Mbps - full-duplex
    Port 1 Link Up - speed 10000 Mbps - full-duplex
    Done

    testpmd> start tx_first
    io packet forwarding - CRC stripping disabled - packets/burst=16
    nb forwarding cores=1 - nb forwarding ports=2
    RX queues=1 - RX desc=128 - RX free threshold=0
    RX threshold registers: pthresh=8 hthresh=8 wthresh=4
    TX queues=1 - TX desc=512 - TX free threshold=0
    TX threshold registers: pthresh=36 hthresh=0 wthresh=0
    TX RS bit threshold=0 - TXQ flags=0x0

    testpmd> stop
    Telling cores to stop...
    Waiting for lcores to finish...

.. image:: img/forward_stats.*

.. code-block:: console

    +++++++++++++++ Accumulated forward statistics for allports++++++++++
    RX-packets: 462384736  RX-dropped: 0 RX-total: 462384736
    TX-packets: 462384768  TX-dropped: 0 TX-total: 462384768
    +++++++++++++++++++++++++++++++++++++++++++++++++++++

    Done.


Using the Poll Mode Driver from an Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Both drivers can provide similar APIs to allow the user to create a PMD, that is,
rte_ethdev structure, instances at run-time in the end-application,
for example, using rte_eth_from_rings() or rte_eth_from_pcaps() APIs.
For the rings-based PMD, this functionality could be used, for example,
to allow data exchange between cores using rings to be done in exactly the
same way as sending or receiving packets from an Ethernet device.
For the libpcap-based PMD, it allows an application to open one or more pcap files
and use these as a source of packet input to the application.

Usage Examples
^^^^^^^^^^^^^^

To create two pseudo-Ethernet ports where all traffic sent to a port is looped back
for reception on the same port (error handling omitted for clarity):

.. code-block:: c

    #define RING_SIZE 256
    #define NUM_RINGS 2
    #define SOCKET0 0

    struct rte_ring *ring[NUM_RINGS];
    int port0, port1;

    ring[0] = rte_ring_create("R0", RING_SIZE, SOCKET0, RING_F_SP_ENQ|RING_F_SC_DEQ);
    ring[1] = rte_ring_create("R1", RING_SIZE, SOCKET0, RING_F_SP_ENQ|RING_F_SC_DEQ);

    /* create two ethdev's */

    port0 = rte_eth_from_rings("net_ring0", ring, NUM_RINGS, ring, NUM_RINGS, SOCKET0);
    port1 = rte_eth_from_rings("net_ring1", ring, NUM_RINGS, ring, NUM_RINGS, SOCKET0);


To create two pseudo-Ethernet ports where the traffic is switched between them,
that is, traffic sent to port 0 is read back from port 1 and vice-versa,
the final two lines could be changed as below:

.. code-block:: c

    port0 = rte_eth_from_rings("net_ring0", &ring[0], 1, &ring[1], 1, SOCKET0);
    port1 = rte_eth_from_rings("net_ring1", &ring[1], 1, &ring[0], 1, SOCKET0);

This type of configuration could be useful in a pipeline model, for example,
where one may want to have inter-core communication using pseudo Ethernet devices rather than raw rings,
for reasons of API consistency.

Enqueuing and dequeuing items from an rte_ring using the rings-based PMD may be slower than using the native rings API.
This is because DPDK Ethernet drivers make use of function pointers to call the appropriate enqueue or dequeue functions,
while the rte_ring specific functions are direct function calls in the code and are often inlined by the compiler.

   Once an ethdev has been created, for either a ring or a pcap-based PMD,
   it should be configured and started in the same way as a regular Ethernet device, that is,
   by calling rte_eth_dev_configure() to set the number of receive and transmit queues,
   then calling rte_eth_rx_queue_setup() / tx_queue_setup() for each of those queues and
   finally calling rte_eth_dev_start() to allow transmission and reception of packets to begin.
