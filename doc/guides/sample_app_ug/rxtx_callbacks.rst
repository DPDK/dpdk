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


RX/TX Callbacks Sample Application
==================================

The RX/TX Callbacks sample application is a packet forwarding application that
demonstrates the use of user defined callbacks on received and transmitted
packets. The application performs a simple latency check, using callbacks, to
determine the time packets spend within the application.

In the sample application a user defined callback is applied to all received
packets to add a timestamp. A separate callback is applied to all packets
prior to transmission to calculate the elapsed time, in CPU cycles.


Compiling the Application
-------------------------

To compile the application export the path to the DPDK source tree and go to
the example directory:

.. code-block:: console

    export RTE_SDK=/path/to/rte_sdk

    cd ${RTE_SDK}/examples/rxtx_callbacks


Set the target, for example:

.. code-block:: console

    export RTE_TARGET=x86_64-native-linuxapp-gcc

See the *DPDK Getting Started* Guide for possible ``RTE_TARGET`` values.

The callbacks feature requires that the ``CONFIG_RTE_ETHDEV_RXTX_CALLBACKS``
setting is on in the ``config/common_`` config file that applies to the
target. This is generally on by default:

.. code-block:: console

    CONFIG_RTE_ETHDEV_RXTX_CALLBACKS=y

Build the application as follows:

.. code-block:: console

    make


Running the Application
-----------------------

To run the example in a ``linuxapp`` environment:

.. code-block:: console

    ./build/rxtx_callbacks -c 2 -n 4

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.



Explanation
-----------

The ``rxtx_callbacks`` application is mainly a simple forwarding application
based on the :doc:`skeleton`. See that section of the documentation for more
details of the forwarding part of the application.

The sections below explain the additional RX/TX callback code.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the application initialization and calls the
execution threads for each lcore. This function is effectively identical to
the ``main()`` function explained in :doc:`skeleton`.

The ``lcore_main()`` function is also identical.

The main difference is in the user defined ``port_init()`` function where the
callbacks are added. This is explained in the next section:


The Port Initialization  Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main functional part of the port initialization is shown below with
comments:

.. code-block:: c

    static inline int
    port_init(uint8_t port, struct rte_mempool *mbuf_pool)
    {
        struct rte_eth_conf port_conf = port_conf_default;
        const uint16_t rx_rings = 1, tx_rings = 1;
        struct ether_addr addr;
        int retval;
        uint16_t q;

        if (port >= rte_eth_dev_count())
            return -1;

        /* Configure the Ethernet device. */
        retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
        if (retval != 0)
            return retval;

        /* Allocate and set up 1 RX queue per Ethernet port. */
        for (q = 0; q < rx_rings; q++) {
            retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                    rte_eth_dev_socket_id(port), NULL, mbuf_pool);
            if (retval < 0)
                return retval;
        }

        /* Allocate and set up 1 TX queue per Ethernet port. */
        for (q = 0; q < tx_rings; q++) {
            retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                    rte_eth_dev_socket_id(port), NULL);
            if (retval < 0)
                return retval;
        }

        /* Start the Ethernet port. */
        retval = rte_eth_dev_start(port);
        if (retval < 0)
            return retval;

        /* Enable RX in promiscuous mode for the Ethernet device. */
        rte_eth_promiscuous_enable(port);


        /* Add the callbacks for RX and TX.*/
        rte_eth_add_rx_callback(port, 0, add_timestamps, NULL);
        rte_eth_add_tx_callback(port, 0, calc_latency, NULL);

        return 0;
    }


The RX and TX callbacks are added to the ports/queues as function pointers:

.. code-block:: c

        rte_eth_add_rx_callback(port, 0, add_timestamps, NULL);
        rte_eth_add_tx_callback(port, 0, calc_latency,   NULL);

More than one callback can be added and additional information can be passed
to callback function pointers as a ``void*``. In the examples above ``NULL``
is used.

The ``add_timestamps()`` and ``calc_latency()`` functions are explained below.


The add_timestamps() Callback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``add_timestamps()`` callback is added to the RX port and is applied to
all packets received:

.. code-block:: c

    static uint16_t
    add_timestamps(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
            struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused)
    {
        unsigned i;
        uint64_t now = rte_rdtsc();

        for (i = 0; i < nb_pkts; i++)
            pkts[i]->udata64 = now;

        return nb_pkts;
    }

The DPDK function ``rte_rdtsc()`` is used to add a cycle count timestamp to
each packet (see the *cycles* section of the *DPDK API Documentation* for
details).


The calc_latency() Callback
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``calc_latency()`` callback is added to the TX port and is applied to all
packets prior to transmission:

.. code-block:: c

    static uint16_t
    calc_latency(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
            struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused)
    {
        uint64_t cycles = 0;
        uint64_t now = rte_rdtsc();
        unsigned i;

        for (i = 0; i < nb_pkts; i++)
            cycles += now - pkts[i]->udata64;

        latency_numbers.total_cycles += cycles;
        latency_numbers.total_pkts   += nb_pkts;

        if (latency_numbers.total_pkts > (100 * 1000 * 1000ULL)) {
            printf("Latency = %"PRIu64" cycles\n",
                    latency_numbers.total_cycles / latency_numbers.total_pkts);

            latency_numbers.total_cycles = latency_numbers.total_pkts = 0;
        }

        return nb_pkts;
    }

The ``calc_latency()`` function accumulates the total number of packets and
the total number of cycles used. Once more than 100 million packets have been
transmitted the average cycle count per packet is printed out and the counters
are reset.
