..  BSD LICENSE
    Copyright(c) 2015 Intel Corporation. All rights reserved.
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


PTP Client Sample Application
=============================

The PTP (Precision Time Protocol) client sample application is a simple
example of using the DPDK IEEE1588 API to communicate with a PTP master clock
to synchronize the time on the NIC and, optionally, on the Linux system.

Note, PTP is a time syncing protocol and cannot be used within DPDK as a
time-stamping mechanism. See the following for an explanation of the protocol:
`Precision Time Protocol
<https://en.wikipedia.org/wiki/Precision_Time_Protocol>`_.


Limitations
-----------

The PTP sample application is intended as a simple reference implementation of
a PTP client using the DPDK IEEE1588 API.
In order to keep the application simple the following assumptions are made:

* The first discovered master is the master for the session.
* Only L2 PTP packets are supported.
* Only the PTP v2 protocol is supported.
* Only the slave clock is implemented.


How the Application Works
-------------------------

.. _figure_ptpclient_highlevel:

.. figure:: img/ptpclient.*

   PTP Synchronization Protocol

The PTP synchronization in the sample application works as follows:

* Master sends *Sync* message - the slave saves it as T2.
* Master sends *Follow Up* message and sends time of T1.
* Slave sends *Delay Request* frame to PTP Master and stores T3.
* Master sends *Delay Response* T4 time which is time of received T3.

The adjustment for slave can be represented as:

   adj = -[(T2-T1)-(T4 - T3)]/2

If the command line parameter ``-T 1`` is used the application also
synchronizes the PTP PHC clock with the Linux kernel clock.


Compiling the Application
-------------------------

To compile the application, export the path to the DPDK source tree and edit
the ``config/common_linuxapp`` configuration file to enable IEEE1588:

.. code-block:: console

    export RTE_SDK=/path/to/rte_sdk

    # Edit  common_linuxapp and set the following options:
    CONFIG_RTE_LIBRTE_IEEE1588=y

Set the target, for example:

.. code-block:: console

    export RTE_TARGET=x86_64-native-linuxapp-gcc

See the *DPDK Getting Started* Guide for possible ``RTE_TARGET`` values.

Build the application as follows:

.. code-block:: console

    # Recompile DPDK.
    make install T=$RTE_TARGET

    # Compile the application.
    cd ${RTE_SDK}/examples/ptpclient
    make


Running the Application
-----------------------

To run the example in a ``linuxapp`` environment:

.. code-block:: console

    ./build/ptpclient -l 1 -n 4 -- -p 0x1 -T 0

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.

* ``-p portmask``: Hexadecimal portmask.
* ``-T 0``: Update only the PTP slave clock.
* ``-T 1``: Update the PTP slave clock and synchronize the Linux Kernel to the PTP clock.


Code Explanation
----------------

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with ``rte_``
and are explained in detail in the *DPDK API Documentation*.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the initialization and calls the execution
threads for each lcore.

The first task is to initialize the Environment Abstraction Layer (EAL).  The
``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. code-block:: c

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

And than we parse application specific arguments

.. code-block:: c

    argc -= ret;
    argv += ret;

    ret = ptp_parse_args(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with PTP initialization\n");

The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. code-block:: c

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
           MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes all the ports using the user defined
``port_init()`` function with portmask provided by user:

.. code-block:: c

    for (portid = 0; portid < nb_ports; portid++)
        if ((ptp_enabled_port_mask & (1 << portid)) != 0) {

            if (port_init(portid, mbuf_pool) == 0) {
                ptp_enabled_ports[ptp_enabled_port_nb] = portid;
                ptp_enabled_port_nb++;
            } else {
                rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n",
                        portid);
            }
        }


Once the initialization is complete, the application is ready to launch a
function on an lcore. In this example ``lcore_main()`` is called on a single
lcore.

.. code-block:: c

	lcore_main();

The ``lcore_main()`` function is explained below.


The Lcores Main
~~~~~~~~~~~~~~~

As we saw above the ``main()`` function calls an application function on the
available lcores.

The main work of the application is done within the loop:

.. code-block:: c

        for (portid = 0; portid < ptp_enabled_port_nb; portid++) {

            portid = ptp_enabled_ports[portid];
            nb_rx = rte_eth_rx_burst(portid, 0, &m, 1);

            if (likely(nb_rx == 0))
                continue;

            if (m->ol_flags & PKT_RX_IEEE1588_PTP)
                parse_ptp_frames(portid, m);

            rte_pktmbuf_free(m);
        }

Packets are received one by one on the RX ports and, if required, PTP response
packets are transmitted on the TX ports.

If the offload flags in the mbuf indicate that the packet is a PTP packet then
the packet is parsed to determine which type:

.. code-block:: c

            if (m->ol_flags & PKT_RX_IEEE1588_PTP)
                 parse_ptp_frames(portid, m);


All packets are freed explicitly using ``rte_pktmbuf_free()``.

The forwarding loop can be interrupted and the application closed using
``Ctrl-C``.


PTP parsing
~~~~~~~~~~~

The ``parse_ptp_frames()`` function processes PTP packets, implementing slave
PTP IEEE1588 L2 functionality.

.. code-block:: c

    void
    parse_ptp_frames(uint8_t portid, struct rte_mbuf *m) {
        struct ptp_header *ptp_hdr;
        struct ether_hdr *eth_hdr;
        uint16_t eth_type;

        eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
        eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);

        if (eth_type == PTP_PROTOCOL) {
            ptp_data.m = m;
            ptp_data.portid = portid;
            ptp_hdr = (struct ptp_header *)(rte_pktmbuf_mtod(m, char *)
                        + sizeof(struct ether_hdr));

            switch (ptp_hdr->msgtype) {
            case SYNC:
                parse_sync(&ptp_data);
                break;
            case FOLLOW_UP:
                parse_fup(&ptp_data);
                break;
            case DELAY_RESP:
                parse_drsp(&ptp_data);
                print_clock_info(&ptp_data);
                break;
            default:
                break;
            }
        }
    }

There are 3 types of packets on the RX path which we must parse to create a minimal
implementation of the PTP slave client:

* SYNC packet.
* FOLLOW UP packet
* DELAY RESPONSE packet.

When we parse the *FOLLOW UP* packet we also create and send a *DELAY_REQUEST* packet.
Also when we parse the *DELAY RESPONSE* packet, and all conditions are met we adjust the PTP slave clock.
