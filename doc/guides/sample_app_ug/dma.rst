..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019-2021 Intel Corporation.

.. include:: <isonum.txt>

Packet copying using DMAdev library
===================================

Overview
--------

This sample is intended as a demonstration of the basic components of a DPDK
forwarding application and example of how to use the DMAdev API to make a packet
copy application.

Also while forwarding, the MAC addresses are affected as follows:

*   The source MAC address is replaced by the TX port MAC address

*   The destination MAC address is replaced by  02:00:00:00:00:TX_PORT_ID

This application can be used to compare performance of using software packet
copy with copy done using a DMA device for different sizes of packets.
The example will print out statistics each second. The stats shows
received/send packets and packets dropped or failed to copy.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``dma`` sub-directory.


Running the Application
-----------------------

In order to run the hardware copy application, the copying device
needs to be bound to user-space IO driver.

Refer to the "DMAdev library" chapter in the "Programmers guide" for information
on using the library.

The application requires a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ioat [EAL options] -- [-p MASK] [-q NQ] [-s RS] [-c <sw|hw>]
        [--[no-]mac-updating] [-b BS] [-f FS] [-i SI]

where,

*   p MASK: A hexadecimal bitmask of the ports to configure (default is all)

*   q NQ: Number of Rx queues used per port equivalent to DMA channels
    per port (default is 1)

*   c CT: Performed packet copy type: software (sw) or hardware using
    DMA (hw) (default is hw)

*   s RS: Size of dmadev descriptor ring for hardware copy mode or rte_ring for
    software copy mode (default is 2048)

*   --[no-]mac-updating: Whether MAC address of packets should be changed
    or not (default is mac-updating)

*   b BS: set the DMA batch size

*   f FS: set the max frame size

*   i SI: set the interval, in second, between statistics prints (default is 1)

The application can be launched in various configurations depending on
provided parameters. The app can use up to 2 lcores: one of them receives
incoming traffic and makes a copy of each packet. The second lcore then
updates MAC address and sends the copy. If one lcore per port is used,
both operations are done sequentially. For each configuration an additional
lcore is needed since the main lcore does not handle traffic but is
responsible for configuration, statistics printing and safe shutdown of
all ports and devices.

The application can use a maximum of 8 ports.

To run the application in a Linux environment with 3 lcores (the main lcore,
plus two forwarding cores), a single port (port 0), software copying and MAC
updating issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-dma -l 0-2 -n 2 -- -p 0x1 --mac-updating -c sw

To run the application in a Linux environment with 2 lcores (the main lcore,
plus one forwarding core), 2 ports (ports 0 and 1), hardware copying and no MAC
updating issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-dma -l 0-1 -n 1 -- -p 0x3 --no-mac-updating -c hw

Refer to the *DPDK Getting Started Guide* for general information on
running applications and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with
``rte_`` and are explained in detail in the *DPDK API Documentation*.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the initialization and calls the execution
threads for each lcore.

The first task is to initialize the Environment Abstraction Layer (EAL).
The ``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Init EAL. 8<
    :end-before: >8 End of init EAL.
    :dedent: 1


The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Allocates mempool to hold the mbufs. 8<
    :end-before: >8 End of allocates mempool to hold the mbufs.
    :dedent: 1

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes the ports:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Initialize each port. 8<
    :end-before: >8 End of initializing each port.
    :dedent: 1

Each port is configured using ``port_init()`` function. The Ethernet
ports are configured with local settings using the ``rte_eth_dev_configure()``
function and the ``port_conf`` struct. The RSS is enabled so that
multiple Rx queues could be used for packet receiving and copying by
multiple DMA channels per port:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Configuring port to use RSS for multiple RX queues. 8<
    :end-before: >8 End of configuring port to use RSS for multiple RX queues.
    :dedent: 1

For this example the ports are set up with the number of Rx queues provided
with -q option and 1 Tx queue using the ``rte_eth_rx_queue_setup()``
and ``rte_eth_tx_queue_setup()`` functions.

The Ethernet port is then started:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Start device. 8<
    :end-before: >8 End of starting device.
    :dedent: 1


Finally the Rx port is set in promiscuous mode:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: RX port is set in promiscuous mode. 8<
    :end-before: >8 End of RX port is set in promiscuous mode.
    :dedent: 1


After that each port application assigns resources needed.

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Assigning each port resources. 8<
    :end-before: >8 End of assigning each port resources.
    :dedent: 1

Ring structures are assigned for exchanging packets between lcores for both SW
and HW copy modes.

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Assign ring structures for packet exchanging. 8<
    :end-before: >8 End of assigning ring structures for packet exchanging.
    :dedent: 0


When using hardware copy each Rx queue of the port is assigned a DMA device
(``assign_dmadevs()``) using DMAdev library API functions:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Using dmadev API functions. 8<
    :end-before: >8 End of using dmadev API functions.
    :dedent: 0


The initialization of hardware device is done by ``rte_dma_configure()`` and
``rte_dma_vchan_setup()`` functions using the ``rte_dma_conf`` and
``rte_dma_vchan_conf`` structs. After configuration the device is started
using ``rte_dma_start()`` function. Each of the above operations is done in
``configure_dmadev_queue()``.

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Configuration of device. 8<
    :end-before: >8 End of configuration of device.
    :dedent: 0

If initialization is successful, memory for hardware device
statistics is allocated.

Finally ``main()`` function starts all packet handling lcores and starts
printing stats in a loop on the main lcore. The application can be
interrupted and closed using ``Ctrl-C``. The main lcore waits for
all worker lcores to finish, deallocates resources and exits.

The processing lcores launching function are described below.

The Lcores Launching Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As described above, ``main()`` function invokes ``start_forwarding_cores()``
function in order to start processing for each lcore:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Start processing for each lcore. 8<
    :end-before: >8 End of starting to process for each lcore.
    :dedent: 0

The function launches Rx/Tx processing functions on configured lcores
using ``rte_eal_remote_launch()``. The configured ports, their number
and number of assigned lcores are stored in user-defined
``rxtx_transmission_config`` struct:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Configuring ports and number of assigned lcores in struct. 8<
    :end-before: >8 End of configuration of ports and number of assigned lcores.
    :dedent: 0

The structure is initialized in 'main()' function with the values
corresponding to ports and lcores configuration provided by the user.

The Lcores Processing Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For receiving packets on each port, the ``dma_rx_port()`` function is used.
The function receives packets on each configured Rx queue. Depending on the
mode the user chose, it will enqueue packets to DMA channels and
then invoke copy process (hardware copy), or perform software copy of each
packet using ``pktmbuf_sw_copy()`` function and enqueue them to an rte_ring:

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Receive packets on one port and enqueue to dmadev or rte_ring. 8<
    :end-before: >8 End of receive packets on one port and enqueue to dmadev or rte_ring.
    :dedent: 0

The packets are received in burst mode using ``rte_eth_rx_burst()``
function. When using hardware copy mode the packets are enqueued in
copying device's buffer using ``dma_enqueue_packets()`` which calls
``rte_dma_copy()``. When all received packets are in the
buffer the copy operations are started by calling ``rte_dma_submit()``.
Function ``rte_dma_copy()`` operates on physical address of
the packet. Structure ``rte_mbuf`` contains only physical address to
start of the data buffer (``buf_iova``). Thus the ``rte_pktmbuf_iova()`` API is
used to get the address of the start of the data within the mbuf.

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Receive packets on one port and enqueue to dmadev or rte_ring. 8<
    :end-before: >8 End of receive packets on one port and enqueue to dmadev or rte_ring.
    :dedent: 0


Once the copies have been completed (this includes gathering the completions in
HW copy mode), the copied packets are enqueued to the ``rx_to_tx_ring``, which
is used to pass the packets to the TX function.

All completed copies are processed by ``dma_tx_port()`` function. This function
dequeues copied packets from the ``rx_to_tx_ring``. Then each packet MAC address is changed
if it was enabled. After that copies are sent in burst mode using ``rte_eth_tx_burst()``.


.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Transmit packets from dmadev/rte_ring for one port. 8<
    :end-before: >8 End of transmitting packets from dmadev.
    :dedent: 0

The Packet Copying Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to perform SW packet copy, there are user-defined functions to first copy
the packet metadata (``pktmbuf_metadata_copy()``) and then the packet data
(``pktmbuf_sw_copy()``):

.. literalinclude:: ../../../examples/dma/dmafwd.c
    :language: c
    :start-after: Perform packet copy there is a user-defined function. 8<
    :end-before: >8 End of perform packet copy there is a user-defined function.
    :dedent: 0

The metadata in this example is copied from ``rx_descriptor_fields1`` marker of
``rte_mbuf`` struct up to ``buf_len`` member.

In order to understand why software packet copying is done as shown
above please refer to the "Mbuf Library" section of the
*DPDK Programmer's Guide*.
