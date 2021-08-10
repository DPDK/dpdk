..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

VMDq Forwarding Sample Application
==========================================

The VMDq Forwarding sample application is a simple example of packet processing using the DPDK.
The application performs L2 forwarding using VMDq to divide the incoming traffic into queues.
The traffic splitting is performed in hardware by the VMDq feature of the Intel® 82599 and X710/XL710 Ethernet Controllers.

Overview
--------

This sample application can be used as a starting point for developing a new application that is based on the DPDK and
uses VMDq for traffic partitioning.

VMDq filters split the incoming packets up into different "pools" - each with its own set of RX queues - based upon
the MAC address and VLAN ID within the VLAN tag of the packet.

All traffic is read from a single incoming port and output on another port, without any processing being performed.
With Intel® 82599 NIC, for example, the traffic is split into 128 queues on input, where each thread of the application reads from
multiple queues. When run with 8 threads, that is, with the -c FF option, each thread receives and forwards packets from 16 queues.

As supplied, the sample application configures the VMDq feature to have 32 pools with 4 queues each.
The Intel® 82599 10 Gigabit Ethernet Controller NIC also supports the splitting of traffic into 16 pools of 2 queues.
While the Intel® X710 or XL710 Ethernet Controller NICs support many configurations of VMDq pools of 4 or 8 queues each.
And queues numbers for each VMDq pool can be changed by setting RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM
in config/rte_config.h file.
The nb-pools and enable-rss parameters can be passed on the command line, after the EAL parameters:

.. code-block:: console

    ./<build_dir>/examples/dpdk-vmdq [EAL options] -- -p PORTMASK --nb-pools NP --enable-rss

where, NP can be 8, 16 or 32, rss is disabled by default.

In Linux* user space, the application can display statistics with the number of packets received on each queue.
To have the application display the statistics, send a SIGHUP signal to the running application process.

The VMDq Forwarding sample application is in many ways simpler than the L2 Forwarding application
(see :doc:`l2_forward_real_virtual`)
as it performs unidirectional L2 forwarding of packets from one port to a second port.
No command-line options are taken by this application apart from the standard EAL command-line options.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``vmdq`` sub-directory.

Running the Application
-----------------------

To run the example in a Linux environment:

.. code-block:: console

    user@target:~$ ./<build_dir>/examples/dpdk-vmdq -l 0-3 -n 4 -- -p 0x3 --nb-pools 16

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

Initialization
~~~~~~~~~~~~~~

The EAL, driver and PCI configuration is performed largely as in the L2 Forwarding sample application,
as is the creation of the mbuf pool.
See :doc:`l2_forward_real_virtual`.
Where this example application differs is in the configuration of the NIC port for RX.

The VMDq hardware feature is configured at port initialization time by setting the appropriate values in the
rte_eth_conf structure passed to the rte_eth_dev_configure() API.
Initially in the application,
a default structure is provided for VMDq configuration to be filled in later by the application.

.. literalinclude:: ../../../examples/vmdq/main.c
    :language: c
    :start-after: Default structure for VMDq. 8<
    :end-before: >8 End of Empty vdmq configuration structure.

The get_eth_conf() function fills in an rte_eth_conf structure with the appropriate values,
based on the global vlan_tags array.
For the VLAN IDs, each one can be allocated to possibly multiple pools of queues.
For destination MAC, each VMDq pool will be assigned with a MAC address. In this sample, each VMDq pool
is assigned to the MAC like 52:54:00:12:<port_id>:<pool_id>, that is,
the MAC of VMDq pool 2 on port 1 is 52:54:00:12:01:02.

.. literalinclude:: ../../../examples/vmdq/main.c
    :language: c
    :start-after: vlan_tags 8<
    :end-before: >8 End of vlan_tags.

.. literalinclude:: ../../../examples/vmdq/main.c
    :language: c
    :start-after: Pool mac address template. 8<
    :end-before: >8 End of mac addr template.

.. literalinclude:: ../../../examples/vmdq/main.c
    :language: c
    :start-after: Building correct configuration for vdmq. 8<
    :end-before: >8 End of get_eth_conf.

Once the network port has been initialized using the correct VMDq values,
the initialization of the port's RX and TX hardware rings is performed similarly to that
in the L2 Forwarding sample application.
See :doc:`l2_forward_real_virtual` for more information.

Statistics Display
~~~~~~~~~~~~~~~~~~

When run in a Linux environment,
the VMDq Forwarding sample application can display statistics showing the number of packets read from each RX queue.
This is provided by way of a signal handler for the SIGHUP signal,
which simply prints to standard output the packet counts in grid form.
Each row of the output is a single pool with the columns being the queue number within that pool.

To generate the statistics output, use the following command:

.. code-block:: console

    user@host$ sudo killall -HUP vmdq_app

Please note that the statistics output will appear on the terminal where the vmdq_app is running,
rather than the terminal from which the HUP signal was sent.
