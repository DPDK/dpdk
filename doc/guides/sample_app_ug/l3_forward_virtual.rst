..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

L3 Forwarding in a Virtualization Environment Sample Application
================================================================

The L3 Forwarding in a Virtualization Environment sample application is a simple example of packet processing using the DPDK.
The application performs L3 forwarding that takes advantage of Single Root I/O Virtualization (SR-IOV) features
in a virtualized environment.

Overview
--------

The application demonstrates the use of the hash and LPM libraries in the DPDK to implement packet forwarding.
The initialization and run-time paths are very similar to those of the :doc:`l3_forward`.
The forwarding decision is taken based on information read from the input packet.

The lookup method is either hash-based or LPM-based and is selected at compile time.
When the selected lookup method is hash-based, a hash object is used to emulate the flow classification stage.
The hash object is used in correlation with the flow table to map each input packet to its flow at runtime.

The hash lookup key is represented by the DiffServ 5-tuple composed of the following fields read from the input packet:
Source IP Address, Destination IP Address, Protocol, Source Port and Destination Port.
The ID of the output interface for the input packet is read from the identified flow table entry.
The set of flows used by the application is statically configured and loaded into the hash at initialization time.
When the selected lookup method is LPM based, an LPM object is used to emulate the forwarding stage for IPv4 packets.
The LPM object is used as the routing table to identify the next hop for each input packet at runtime.

The LPM lookup key is represented by the Destination IP Address field read from the input packet.
The ID of the output interface for the input packet is the next hop returned by the LPM lookup.
The set of LPM rules used by the application is statically configured and loaded into the LPM object at the initialization time.

.. note::

    Please refer to :ref:`l2_fwd_vf_setup` for virtualized test case setup.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``l3fwd-vf`` sub-directory.

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

    ./build/l3fwd-vf [EAL options] -- -p PORTMASK  --config(port,queue,lcore)[,(port,queue,lcore)] [--no-numa]

where,

*   --p PORTMASK: Hexadecimal bitmask of ports to configure

*   --config (port,queue,lcore)[,(port,queue,lcore]: determines which queues from which ports are mapped to which cores

*   --no-numa: optional, disables numa awareness

For example, consider a dual processor socket platform with 8 physical cores, where cores 0-7 and 16-23 appear on socket 0,
while cores 8-15 and 24-31 appear on socket 1.

To enable L3 forwarding between two ports, assuming that both ports are in the same socket, using two cores, cores 1 and 2,
(which are in the same socket too), use the following command:

.. code-block:: console

   ./build/l3fwd-vf -l 1,2 -n 4 -- -p 0x3 --config="(0,0,1),(1,0,2)"

In this command:

*   The -l option enables cores 1 and 2

*   The -p option enables ports 0 and 1

*   The --config option enables one queue on each port and maps each (port,queue) pair to a specific core.
    The following table shows the mapping in this example:

    +----------+-----------+-----------+------------------------------------+
    | **Port** | **Queue** | **lcore** | **Description**                    |
    |          |           |           |                                    |
    +==========+===========+===========+====================================+
    | 0        | 0         | 1         | Map queue 0 from port 0 to lcore 1 |
    |          |           |           |                                    |
    +----------+-----------+-----------+------------------------------------+
    | 1        | 0         | 2         | Map queue 0 from port 1 to lcore 2 |
    |          |           |           |                                    |
    +----------+-----------+-----------+------------------------------------+

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The operation of this application is similar to that of the basic L3 Forwarding Sample Application.
See :ref:`l3_fwd_explanation` for more information.
