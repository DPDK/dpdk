..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Link Status Interrupt Sample Application
========================================

The Link Status Interrupt sample application is a simple example of packet processing using
the Data Plane Development Kit (DPDK) that
demonstrates how network link status changes for a network port can be captured and
used by a DPDK application.

Overview
--------

The Link Status Interrupt sample application registers a user space callback for the link status interrupt of each port
and performs L2 forwarding for each packet that is received on an RX_PORT.
The following operations are performed:

*   RX_PORT and TX_PORT are paired with available ports one-by-one according to the core mask

*   The source MAC address is replaced by the TX_PORT MAC address

*   The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID

This application can be used to demonstrate the usage of link status interrupt and its user space callbacks
and the behavior of L2 forwarding each time the link status changes.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``link_status_interrupt`` sub-directory.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-link_status_interrupt [EAL options] -- -p PORTMASK [-q NQ][-T PERIOD]

where,

*   -p PORTMASK: A hexadecimal bitmask of the ports to configure

*   -q NQ: A number of queues (=ports) per lcore (default is 1)

*   -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default)

To run the application in a linux environment with 4 lcores, 4 memory channels, 16 ports and 8 RX queues per lcore,
issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-link_status_interrupt -l 0-3 -n 4-- -q 8 -p ffff

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

Command Line Arguments
~~~~~~~~~~~~~~~~~~~~~~

The Link Status Interrupt sample application takes specific parameters,
in addition to Environment Abstraction Layer (EAL) arguments (see Section `Running the Application`_).

Command line parsing is done in the same way as it is done in the L2 Forwarding Sample Application.
See :ref:`l2_fwd_app_cmd_arguments` for more information.

Mbuf Pool Initialization
~~~~~~~~~~~~~~~~~~~~~~~~

Mbuf pool initialization is done in the same way as it is done in the L2 Forwarding Sample Application.
See :ref:`l2_fwd_app_mbuf_init` for more information.

Driver Initialization
~~~~~~~~~~~~~~~~~~~~~

The main part of the code in the main() function relates to the initialization of the driver.
To fully understand this code, it is recommended to study the chapters that related to the Poll Mode Driver in the
*DPDK Programmer's Guide and the DPDK API Reference*.

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: Each logical core is assigned a dedicated TX queue on each port. 8<
        :end-before: >8 End of assigning logical core.
        :dedent: 1

The next step is to configure the RX and TX queues.
For each port, there is only one RX queue (only one lcore is able to poll a given port).
The number of TX queues depends on the number of available lcores.
The rte_eth_dev_configure() function is used to configure the number of queues for a port:

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: Configure RX and TX queues. 8<
        :end-before: >8 End of configure RX and TX queues.
        :dedent: 2

The global configuration is stored in a static structure:

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: Global configuration stored in a static structure. 8<
        :end-before: >8 End of global configuration stored in a static structure.

Configuring lsc to 0 (the default) disables the generation of any link status change interrupts in kernel space
and no user space interrupt event is received.
The public interface rte_eth_link_get() accesses the NIC registers directly to update the link status.
Configuring lsc to non-zero enables the generation of link status change interrupts in kernel space
when a link status change is present and calls the user space callbacks registered by the application.
The public interface rte_eth_link_get() just reads the link status in a global structure
that would be updated in the interrupt host thread only.

Interrupt Callback Registration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application can register one or more callbacks to a specific port and interrupt event.
An example callback function that has been written as indicated below.

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: lsi_event_callback 8<
        :end-before: >8 End of registering one or more callbacks.

This function is called when a link status interrupt is present for the right port.
The port_id indicates which port the interrupt applies to.
The type parameter identifies the interrupt event type,
which currently can be RTE_ETH_EVENT_INTR_LSC only, but other types can be added in the future.
The param parameter is the address of the parameter for the callback.
This function should be implemented with care since it will be called in the interrupt host thread,
which is different from the main thread of its caller.

The application registers the lsi_event_callback and a NULL parameter to the link status interrupt event on each port:

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: RTE callback register. 8<
        :end-before: >8 End of registering lsi interrupt callback.
        :dedent: 2

This registration can be done only after calling the rte_eth_dev_configure() function and before calling any other function.
If lsc is initialized with 0, the callback is never called since no interrupt event would ever be present.

RX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

The application uses one lcore to poll one or several ports, depending on the -q option,
which specifies the number of queues per lcore.

For example, if the user specifies -q 4, the application is able to poll four ports with one lcore.
If there are 16 ports on the target (and if the portmask argument is -p ffff),
the application will need four lcores to poll all the ports.

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: RX queue initialization. 8<
        :end-before: >8 End of RX queue initialization.
        :dedent: 2

The list of queues that must be polled for a given lcore is stored in a private structure called struct lcore_queue_conf.

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: List of queues must be polled for a give lcore. 8<
        :end-before: >8 End of list of queues to be polled.

The n_rx_port and rx_port_list[] fields are used in the main packet processing loop
(see `Receive, Process and Transmit Packets`_).

The global configuration for the RX queues is stored in a static structure:

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: List of queues must be polled for a give lcore. 8<
        :end-before: >8 End of list of queues to be polled.

TX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

Each lcore should be able to transmit on any port.
For every port, a single TX queue is initialized.

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: init one TX queue logical core on each port. 8<
        :end-before: >8 End of init one TX queue.
        :dedent: 2

The global configuration for TX queues is stored in a static structure:

.. code-block:: c

    static const struct rte_eth_txconf tx_conf = {
        .tx_thresh = {
            .pthresh = TX_PTHRESH,
            .hthresh = TX_HTHRESH,
            .wthresh = TX_WTHRESH,
        },
        .tx_free_thresh = RTE_TEST_TX_DESC_DEFAULT + 1, /* disable feature */
    };

Receive, Process and Transmit Packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the lsi_main_loop() function, the main task is to read ingress packets from the RX queues.
This is done using the following code:

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: Read packet from RX queues. 8<
        :end-before: >8 End of reading packet from RX queues.
        :dedent: 2

Packets are read in a burst of size MAX_PKT_BURST.
The rte_eth_rx_burst() function writes the mbuf pointers in a local table and returns the number of available mbufs in the table.

Then, each mbuf in the table is processed by the lsi_simple_forward() function.
The processing is very simple: processes the TX port from the RX port and then replaces the source and destination MAC addresses.

.. note::

    In the following code, the two lines for calculating the output port require some explanation.
    If portId is even, the first line does nothing (as portid & 1 will be 0), and the second line adds 1.
    If portId is odd, the first line subtracts one and the second line does nothing.
    Therefore, 0 goes to 1, and 1 to 0, 2 goes to 3 and 3 to 2, and so on.

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: Replacing the source and destination MAC addresses. 8<
        :end-before: >8 End of replacing the source and destination MAC addresses.

Then, the packet is sent using the lsi_send_packet(m, dst_port) function.
For this test application, the processing is exactly the same for all packets arriving on the same RX port.
Therefore, it would have been possible to call the lsi_send_burst() function directly from the main loop
to send all the received packets on the same TX port using
the burst-oriented send function, which is more efficient.

However, in real-life applications (such as, L3 routing),
packet N is not necessarily forwarded on the same port as packet N-1.
The application is implemented to illustrate that so the same approach can be reused in a more complex application.

The lsi_send_packet() function stores the packet in a per-lcore and per-txport table.
If the table is full, the whole packets table is transmitted using the lsi_send_burst() function:

.. literalinclude:: ../../../examples/l2fwd-crypto/main.c
    :language: c
    :start-after: Enqueue packets for TX and prepare them to be sent. 8<
    :end-before: >8 End of Enqueuing packets for TX.

To ensure that no packets remain in the tables, each lcore does a draining of the TX queue in its main loop.
This technique introduces some latency when there are not many packets to send.
However, it improves performance:

.. literalinclude:: ../../../examples/link_status_interrupt/main.c
        :language: c
        :start-after: Draining TX queue in its main loop. 8<
        :end-before: >8 End of draining TX queue in its main loop.
        :dedent: 2
