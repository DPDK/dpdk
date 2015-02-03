..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

VMDQ and DCB Forwarding Sample Application
==========================================

The VMDQ and DCB Forwarding sample application is a simple example of packet processing using the DPDK.
The application performs L2 forwarding using VMDQ and DCB to divide the incoming traffic into 128 queues.
The traffic splitting is performed in hardware by the VMDQ and DCB features of the Intel® 82599 10 Gigabit Ethernet Controller.

Overview
--------

This sample application can be used as a starting point for developing a new application that is based on the DPDK and
uses VMDQ and DCB for traffic partitioning.

The VMDQ and DCB filters work on VLAN traffic to divide the traffic into 128 input queues on the basis of the VLAN ID field and
VLAN user priority field.
VMDQ filters split the traffic into 16 or 32 groups based on the VLAN ID.
Then, DCB places each packet into one of either 4 or 8 queues within that group, based upon the VLAN user priority field.

In either case, 16 groups of 8 queues, or 32 groups of 4 queues, the traffic can be split into 128 hardware queues on the NIC,
each of which can be polled individually by a DPDK application.

All traffic is read from a single incoming port (port 0) and output on port 1, without any processing being performed.
The traffic is split into 128 queues on input, where each thread of the application reads from multiple queues.
For example, when run with 8 threads, that is, with the -c FF option, each thread receives and forwards packets from 16 queues.

As supplied, the sample application configures the VMDQ feature to have 16 pools with 8 queues each as indicated in Figure 15.
The Intel® 82599 10 Gigabit Ethernet Controller NIC also supports the splitting of traffic into 32 pools of 4 queues each and
this can be used by changing the NUM_POOLS parameter in the supplied code.
The NUM_POOLS parameter can be passed on the command line, after the EAL parameters:

.. code-block:: console

    ./build/vmdq_dcb [EAL options] -- -p PORTMASK --nb-pools NP

where, NP can be 16 or 32.

.. _figure_15:

**Figure 15. Packet Flow Through the VMDQ and DCB Sample Application**

.. image18_png has been replaced

|vmdq_dcb_example|

In Linux* user space, the application can display statistics with the number of packets received on each queue.
To have the application display the statistics, send a SIGHUP signal to the running application process, as follows:

where, <pid> is the process id of the application process.

The VMDQ and DCB Forwarding sample application is in many ways simpler than the L2 Forwarding application
(see Chapter 9 , "L2 Forwarding Sample Application (in Real and Virtualized Environments)")
as it performs unidirectional L2 forwarding of packets from one port to a second port.
No command-line options are taken by this application apart from the standard EAL command-line options.

.. note::

    Since VMD queues are being used for VMM, this application works correctly
    when VTd is disabled in the BIOS or Linux* kernel (intel_iommu=off).

Compiling the Application
-------------------------

#.  Go to the examples directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk cd ${RTE_SDK}/examples/vmdq_dcb

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

To run the example in a linuxapp environment:

.. code-block:: console

    user@target:~$ ./build/vmdq_dcb -c f -n 4 -- -p 0x3 --nb-pools 16

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

Initialization
~~~~~~~~~~~~~~

The EAL, driver and PCI configuration is performed largely as in the L2 Forwarding sample application,
as is the creation of the mbuf pool.
See Chapter 9, "L2 Forwarding Sample Application (in Real and Virtualized Environments)".
Where this example application differs is in the configuration of the NIC port for RX.

The VMDQ and DCB hardware feature is configured at port initialization time by setting the appropriate values in the
rte_eth_conf structure passed to the rte_eth_dev_configure() API.
Initially in the application,
a default structure is provided for VMDQ and DCB configuration to be filled in later by the application.

.. code-block:: c

    /* empty vmdq+dcb configuration structure. Filled in programatically */

    static const struct rte_eth_conf vmdq_dcb_conf_default = {
        .rxmode = {
            .mq_mode = ETH_VMDQ_DCB,
            .split_hdr_size = 0,
            .header_split = 0,   /**< Header Split disabled */
            .hw_ip_checksum = 0, /**< IP checksum offload disabled */
            .hw_vlan_filter = 0, /**< VLAN filtering disabled */
           .jumbo_frame = 0,     /**< Jumbo Frame Support disabled */
        },

        .txmode = {
            .mq_mode = ETH_DCB_NONE,
        },

        .rx_adv_conf = {
            /*
             *    should be overridden separately in code with
             *    appropriate values
             */

            .vmdq_dcb_conf = {
                .nb_queue_pools = ETH_16_POOLS,
                .enable_default_pool = 0,
                .default_pool = 0,
                .nb_pool_maps = 0,
                .pool_map = {{0, 0},},
                .dcb_queue = {0},
            },
        },
    };

The get_eth_conf() function fills in an rte_eth_conf structure with the appropriate values,
based on the global vlan_tags array,
and dividing up the possible user priority values equally among the individual queues
(also referred to as traffic classes) within each pool, that is,
if the number of pools is 32, then the user priority fields are allocated two to a queue.
If 16 pools are used, then each of the 8 user priority fields is allocated to its own queue within the pool.
For the VLAN IDs, each one can be allocated to possibly multiple pools of queues,
so the pools parameter in the rte_eth_vmdq_dcb_conf structure is specified as a bitmask value.

.. code-block:: c

    const uint16_t vlan_tags[] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31
    };


    /* Builds up the correct configuration for vmdq+dcb based on the vlan tags array
     * given above, and the number of traffic classes available for use. */

    static inline int
    get_eth_conf(struct rte_eth_conf *eth_conf, enum rte_eth_nb_pools num_pools)
    {
        struct rte_eth_vmdq_dcb_conf conf;
        unsigned i;

        if (num_pools != ETH_16_POOLS && num_pools != ETH_32_POOLS ) return -1;

        conf.nb_queue_pools = num_pools;
        conf.enable_default_pool = 0;
        conf.default_pool = 0; /* set explicit value, even if not used */
        conf.nb_pool_maps = sizeof( vlan_tags )/sizeof( vlan_tags[ 0 ]);

        for (i = 0; i < conf.nb_pool_maps; i++){
            conf.pool_map[i].vlan_id = vlan_tags[ i ];
            conf.pool_map[i].pools = 1 << (i % num_pools);
        }

        for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++){
            conf.dcb_queue[i] = (uint8_t)(i % (NUM_QUEUES/num_pools));
        }

        (void) rte_memcpy(eth_conf, &vmdq_dcb_conf_default, sizeof(\*eth_conf));
        (void) rte_memcpy(&eth_conf->rx_adv_conf.vmdq_dcb_conf, &conf, sizeof(eth_conf->rx_adv_conf.vmdq_dcb_conf));

        return 0;
    }

Once the network port has been initialized using the correct VMDQ and DCB values,
the initialization of the port's RX and TX hardware rings is performed similarly to that
in the L2 Forwarding sample application.
See Chapter 9, "L2 Forwarding Sample Aplication (in Real and Virtualized Environments)" for more information.

Statistics Display
~~~~~~~~~~~~~~~~~~

When run in a linuxapp environment,
the VMDQ and DCB Forwarding sample application can display statistics showing the number of packets read from each RX queue.
This is provided by way of a signal handler for the SIGHUP signal,
which simply prints to standard output the packet counts in grid form.
Each row of the output is a single pool with the columns being the queue number within that pool.

To generate the statistics output, use the following command:

.. code-block:: console

    user@host$ sudo killall -HUP vmdq_dcb_app

Please note that the statistics output will appear on the terminal where the vmdq_dcb_app is running,
rather than the terminal from which the HUP signal was sent.

.. |vmdq_dcb_example| image:: img/vmdq_dcb_example.*
