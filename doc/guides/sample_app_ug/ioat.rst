..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

.. include:: <isonum.txt>

Packet copying using Intel\ |reg| QuickData Technology
======================================================

Overview
--------

This sample is intended as a demonstration of the basic components of a DPDK
forwarding application and example of how to use IOAT driver API to make
packets copies.

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

The application is located in the ``ioat`` sub-directory.


Running the Application
-----------------------

In order to run the hardware copy application, the copying device
needs to be bound to user-space IO driver.

Refer to the "IOAT Rawdev Driver" chapter in the "Rawdev Drivers" document
for information on using the driver.

The application requires a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ioat [EAL options] -- [-p MASK] [-q NQ] [-s RS] [-c <sw|hw>]
        [--[no-]mac-updating]

where,

*   p MASK: A hexadecimal bitmask of the ports to configure (default is all)

*   q NQ: Number of Rx queues used per port equivalent to CBDMA channels
    per port (default is 1)

*   c CT: Performed packet copy type: software (sw) or hardware using
    DMA (hw) (default is hw)

*   s RS: Size of IOAT rawdev ring for hardware copy mode or rte_ring for
    software copy mode (default is 2048)

*   --[no-]mac-updating: Whether MAC address of packets should be changed
    or not (default is mac-updating)

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

    $ ./<build_dir>/examples/dpdk-ioat -l 0-2 -n 2 -- -p 0x1 --mac-updating -c sw

To run the application in a Linux environment with 2 lcores (the main lcore,
plus one forwarding core), 2 ports (ports 0 and 1), hardware copying and no MAC
updating issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-ioat -l 0-1 -n 1 -- -p 0x3 --no-mac-updating -c hw

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

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Init EAL. 8<
    :end-before: >8 End of init EAL.
    :dedent: 1


The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Allocates mempool to hold the mbufs. 8<
    :end-before: >8 End of allocates mempool to hold the mbufs.
    :dedent: 1

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes the ports:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Initialize each port. 8<
    :end-before: >8 End of initializing each port.
    :dedent: 1

Each port is configured using ``port_init()`` function. The Ethernet
ports are configured with local settings using the ``rte_eth_dev_configure()``
function and the ``port_conf`` struct. The RSS is enabled so that
multiple Rx queues could be used for packet receiving and copying by
multiple CBDMA channels per port:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Configuring port to use RSS for multiple RX queues. 8<
    :end-before: >8 End of configuring port to use RSS for multiple RX queues.
    :dedent: 1

For this example the ports are set up with the number of Rx queues provided
with -q option and 1 Tx queue using the ``rte_eth_rx_queue_setup()``
and ``rte_eth_tx_queue_setup()`` functions.

The Ethernet port is then started:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Start device. 8<
    :end-before: >8 End of starting device.
    :dedent: 1


Finally the Rx port is set in promiscuous mode:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: RX port is set in promiscuous mode. 8<
    :end-before: >8 End of RX port is set in promiscuous mode.
    :dedent: 1


After that each port application assigns resources needed.

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Assigning each port resources. 8<
    :end-before: >8 End of assigning each port resources.
    :dedent: 1

Depending on mode set (whether copy should be done by software or by hardware)
special structures are assigned to each port. If software copy was chosen,
application have to assign ring structures for packet exchanging between lcores
assigned to ports.

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Assign ring structures for packet exchanging. 8<
    :end-before: >8 End of assigning ring structures for packet exchanging.
    :dedent: 0


When using hardware copy each Rx queue of the port is assigned an
IOAT device (``assign_rawdevs()``) using IOAT Rawdev Driver API
functions:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Using IOAT rawdev API functions. 8<
    :end-before: >8 End of using IOAT rawdev API functions.
    :dedent: 0


The initialization of hardware device is done by ``rte_rawdev_configure()``
function using ``rte_rawdev_info`` struct. After configuration the device is
started using ``rte_rawdev_start()`` function. Each of the above operations
is done in ``configure_rawdev_queue()``.

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
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

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Start processing for each lcore. 8<
    :end-before: >8 End of starting to processfor each lcore.
    :dedent: 0

The function launches Rx/Tx processing functions on configured lcores
using ``rte_eal_remote_launch()``. The configured ports, their number
and number of assigned lcores are stored in user-defined
``rxtx_transmission_config`` struct:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Configuring ports and number of assigned lcores in struct. 8<
    :end-before: >8 End of configuration of ports and number of assigned lcores.
    :dedent: 0

The structure is initialized in 'main()' function with the values
corresponding to ports and lcores configuration provided by the user.

The Lcores Processing Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For receiving packets on each port, the ``ioat_rx_port()`` function is used.
The function receives packets on each configured Rx queue. Depending on the
mode the user chose, it will enqueue packets to IOAT rawdev channels and
then invoke copy process (hardware copy), or perform software copy of each
packet using ``pktmbuf_sw_copy()`` function and enqueue them to an rte_ring:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Receive packets on one port and enqueue to IOAT rawdev or rte_ring. 8<
    :end-before: >8 End of receive packets on one port and enqueue to IOAT rawdev or rte_ring.
    :dedent: 0

The packets are received in burst mode using ``rte_eth_rx_burst()``
function. When using hardware copy mode the packets are enqueued in
copying device's buffer using ``ioat_enqueue_packets()`` which calls
``rte_ioat_enqueue_copy()``. When all received packets are in the
buffer the copy operations are started by calling ``rte_ioat_perform_ops()``.
Function ``rte_ioat_enqueue_copy()`` operates on physical address of
the packet. Structure ``rte_mbuf`` contains only physical address to
start of the data buffer (``buf_iova``). Thus the address is adjusted
by ``addr_offset`` value in order to get the address of ``rearm_data``
member of ``rte_mbuf``. That way both the packet data and metadata can
be copied in a single operation. This method can be used because the mbufs
are direct mbufs allocated by the apps. If another app uses external buffers,
or indirect mbufs, then multiple copy operations must be used.

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Receive packets on one port and enqueue to IOAT rawdev or rte_ring. 8<
    :end-before: >8 End of receive packets on one port and enqueue to IOAT rawdev or rte_ring.
    :dedent: 0


All completed copies are processed by ``ioat_tx_port()`` function. When using
hardware copy mode the function invokes ``rte_ioat_completed_ops()``
on each assigned IOAT channel to gather copied packets. If software copy
mode is used the function dequeues copied packets from the rte_ring. Then each
packet MAC address is changed if it was enabled. After that copies are sent
in burst mode using `` rte_eth_tx_burst()``.


.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Transmit packets from IOAT rawdev/rte_ring for one port. 8<
    :end-before: >8 End of transmitting packets from IOAT.
    :dedent: 0

The Packet Copying Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to perform packet copy there is a user-defined function
``pktmbuf_sw_copy()`` used. It copies a whole packet by copying
metadata from source packet to new mbuf, and then copying a data
chunk of source packet. Both memory copies are done using
``rte_memcpy()``:

.. literalinclude:: ../../../examples/ioat/ioatfwd.c
    :language: c
    :start-after: Perform packet copy there is a user-defined function. 8<
    :end-before: >8 End of perform packet copy there is a user-defined function.
    :dedent: 0

The metadata in this example is copied from ``rearm_data`` member of
``rte_mbuf`` struct up to ``cacheline1``.

In order to understand why software packet copying is done as shown
above please refer to the "Mbuf Library" section of the
*DPDK Programmer's Guide*.
