..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Exception Path Sample Application
=================================

The Exception Path sample application is a simple example that demonstrates the use of the DPDK
to set up an exception path for packets to go through the Linux* kernel.
This is done by using virtual TAP network interfaces.
These can be read from and written to by the DPDK application and
appear to the kernel as a standard network interface.

Overview
--------

The application creates two threads for each NIC port being used.
One thread reads from the port and writes the data unmodified to a thread-specific TAP interface.
The second thread reads from a TAP interface and writes the data unmodified to the NIC port.

The packet flow through the exception path application is as shown in the following figure.

.. _figure_exception_path_example:

.. figure:: img/exception_path_example.*

   Packet Flow


To make throughput measurements, kernel bridges must be setup to forward data between the bridges appropriately.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``exception_path`` sub-directory.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    .build/exception_path [EAL options] -- -p PORTMASK -i IN_CORES -o OUT_CORES

where:

*   -p PORTMASK: A hex bitmask of ports to use

*   -i IN_CORES: A hex bitmask of cores which read from NIC

*   -o OUT_CORES: A hex bitmask of cores which write to NIC

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

The number of bits set in each bitmask must be the same.
The coremask -c or the corelist -l parameter of the EAL options should include IN_CORES and OUT_CORES.
The same bit must not be set in IN_CORES and OUT_CORES.
The affinities between ports and cores are set beginning with the least significant bit of each mask, that is,
the port represented by the lowest bit in PORTMASK is read from by the core represented by the lowest bit in IN_CORES,
and written to by the core represented by the lowest bit in OUT_CORES.

For example to run the application with two ports and four cores:

.. code-block:: console

    ./build/exception_path -l 0-3 -n 4 -- -p 3 -i 3 -o c

Getting Statistics
~~~~~~~~~~~~~~~~~~

While the application is running, statistics on packets sent and
received can be displayed by sending the SIGUSR1 signal to the application from another terminal:

.. code-block:: console

    killall -USR1 exception_path

The statistics can be reset by sending a SIGUSR2 signal in a similar way.

Explanation
-----------

The following sections provide some explanation of the code.

Initialization
~~~~~~~~~~~~~~

Setup of the mbuf pool, driver and queues is similar to the setup done in the :ref:`l2_fwd_app_real_and_virtual`.
In addition, the TAP interfaces must also be created.
A TAP interface is created for each lcore that is being used.
The code for creating the TAP interface is as follows:

.. code-block:: c

    /*
     *   Create a tap network interface, or use existing one with same name.
     *   If name[0]='\0' then a name is automatically assigned and returned in name.
     */

    static int tap_create(char *name)
    {
        struct ifreq ifr;
        int fd, ret;

        fd = open("/dev/net/tun", O_RDWR);
        if (fd < 0)
            return fd;

        memset(&ifr, 0, sizeof(ifr));

        /* TAP device without packet information */

        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
        if (name && *name)
            rte_snprinf(ifr.ifr_name, IFNAMSIZ, name);

        ret = ioctl(fd, TUNSETIFF, (void *) &ifr);

        if (ret < 0) {
            close(fd);
            return ret;

        }

        if (name)
            snprintf(name, IFNAMSIZ, ifr.ifr_name);

        return fd;
    }

The other step in the initialization process that is unique to this sample application
is the association of each port with two cores:

*   One core to read from the port and write to a TAP interface

*   A second core to read from a TAP interface and write to the port

This is done using an array called port_ids[], which is indexed by the lcore IDs.
The population of this array is shown below:

.. code-block:: c

    tx_port = 0;
    rx_port = 0;

    RTE_LCORE_FOREACH(i) {
        if (input_cores_mask & (1ULL << i)) {
            /* Skip ports that are not enabled */
            while ((ports_mask & (1 << rx_port)) == 0) {
                rx_port++;
                if (rx_port > (sizeof(ports_mask) * 8))
                    goto fail; /* not enough ports */
            }
            port_ids[i] = rx_port++;
        } else if (output_cores_mask & (1ULL << i)) {
            /* Skip ports that are not enabled */
            while ((ports_mask & (1 << tx_port)) == 0) {
                tx_port++;
                if (tx_port > (sizeof(ports_mask) * 8))
                   goto fail; /* not enough ports */
            }
            port_ids[i] = tx_port++;
        }
   }

Packet Forwarding
~~~~~~~~~~~~~~~~~

After the initialization steps are complete, the main_loop() function is run on each lcore.
This function first checks the lcore_id against the user provided input_cores_mask and output_cores_mask to see
if this core is reading from or writing to a TAP interface.

For the case that reads from a NIC port, the packet reception is the same as in the L2 Forwarding sample application
(see :ref:`l2_fwd_app_rx_tx_packets`).
The packet transmission is done by calling write() with the file descriptor of the appropriate TAP interface
and then explicitly freeing the mbuf back to the pool.

..  code-block:: c

    /* Loop forever reading from NIC and writing to tap */

    for (;;) {
        struct rte_mbuf *pkts_burst[PKT_BURST_SZ];
        unsigned i;

        const unsigned nb_rx = rte_eth_rx_burst(port_ids[lcore_id], 0, pkts_burst, PKT_BURST_SZ);

        lcore_stats[lcore_id].rx += nb_rx;

        for (i = 0; likely(i < nb_rx); i++) {
            struct rte_mbuf *m = pkts_burst[i];
            int ret = write(tap_fd, rte_pktmbuf_mtod(m, void*),

            rte_pktmbuf_data_len(m));
            rte_pktmbuf_free(m);
            if (unlikely(ret<0))
                lcore_stats[lcore_id].dropped++;
            else
                lcore_stats[lcore_id].tx++;
        }
    }

For the other case that reads from a TAP interface and writes to a NIC port,
packets are retrieved by doing a read() from the file descriptor of the appropriate TAP interface.
This fills in the data into the mbuf, then other fields are set manually.
The packet can then be transmitted as normal.

.. code-block:: c

    /* Loop forever reading from tap and writing to NIC */

    for (;;) {
        int ret;
        struct rte_mbuf *m = rte_pktmbuf_alloc(pktmbuf_pool);

        if (m == NULL)
            continue;

        ret = read(tap_fd, m->pkt.data, MAX_PACKET_SZ); lcore_stats[lcore_id].rx++;
        if (unlikely(ret < 0)) {
            FATAL_ERROR("Reading from %s interface failed", tap_name);
        }

        m->pkt.nb_segs = 1;
        m->pkt.next = NULL;
        m->pkt.data_len = (uint16_t)ret;

        ret = rte_eth_tx_burst(port_ids[lcore_id], 0, &m, 1);
        if (unlikely(ret < 1)) {
            rte_pktmuf_free(m);
            lcore_stats[lcore_id].dropped++;
        }
        else {
            lcore_stats[lcore_id].tx++;
        }
    }

To set up loops for measuring throughput, TAP interfaces can be connected using bridging.
The steps to do this are described in the section that follows.

Managing TAP Interfaces and Bridges
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Exception Path sample application creates TAP interfaces with names of the format tap_dpdk_nn,
where nn is the lcore ID. These TAP interfaces need to be configured for use:

.. code-block:: console

    ifconfig tap_dpdk_00 up

To set up a bridge between two interfaces so that packets sent to one interface can be read from another,
use the brctl tool:

.. code-block:: console

    brctl addbr "br0"
    brctl addif br0 tap_dpdk_00
    brctl addif br0 tap_dpdk_03
    ifconfig br0 up

The TAP interfaces created by this application exist only when the application is running,
so the steps above need to be repeated each time the application is run.
To avoid this, persistent TAP interfaces can be created using openvpn:

.. code-block:: console

    openvpn --mktun --dev tap_dpdk_00

If this method is used, then the steps above have to be done only once and
the same TAP interfaces can be reused each time the application is run.
To remove bridges and persistent TAP interfaces, the following commands are used:

.. code-block:: console

    ifconfig br0 down
    brctl delbr br0
    openvpn --rmtun --dev tap_dpdk_00

