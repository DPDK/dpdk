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

L3 Forwarding in a Virtualization Environment Sample Application
================================================================

The L3 Forwarding in a Virtualization Environment sample application is a simple example of packet processing using the DPDK.
The application performs L3 forwarding that takes advantage of Single Root I/O Virtualization (SR-IOV) features
in a virtualized environment.

Overview
--------

The application demonstrates the use of the hash and LPM libraries in the DPDK to implement packet forwarding.
The initialization and run-time paths are very similar to those of the L3 forwarding application
(see Chapter 10 "L3 Forwarding Sample Application" for more information).
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

    Please refer to Section 9.1.1 "Virtual Function Setup Instructions" for virtualized test case setup.

Compiling the Application
-------------------------

To compile the application:

#.  Go to the sample application directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/l3fwd-vf

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

.. note::

    The compiled application is written to the build subdirectory.
    To have the application written to a different location,
    the O=/path/to/build/directory option may be specified in the make command.

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

    ./build/l3fwd-vf [EAL options] -- -p PORTMASK  --config(port,queue,lcore)[,(port,queue,lcore)] [--no-numa]

where,

*   --p PORTMASK: Hexadecimal bitmask of ports to configure

*   --config (port,queue,lcore)[,(port,queue,lcore]: determines which queues from which ports are mapped to which cores

*   --no-numa: optional, disables numa awareness

For example, consider a dual processor socket platform where cores 0,2,4,6, 8, and 10 appear on socket 0,
while cores 1,3,5,7,9, and 11 appear on socket 1.
Let's say that the programmer wants to use memory from both NUMA nodes,
the platform has only two ports and the programmer wants to use one core from each processor socket to do the packet processing
since only one Rx/Tx queue pair can be used in virtualization mode.

To enable L3 forwarding between two ports, using one core from each processor,
while also taking advantage of local memory accesses by optimizing around NUMA,
the programmer can pin to the appropriate cores and allocate memory from the appropriate NUMA node.
This is achieved using the following command:

.. code-block:: console

   ./build/l3fwd-vf -c 0x03 -n 3 -- -p 0x3 --config="(0,0,0),(1,0,1)"

In this command:

*   The -c option enables cores 0 and 1

*   The -p option enables ports 0 and 1

*   The --config option enables one queue on each port and maps each (port,queue) pair to a specific core.
    Logic to enable multiple RX queues using RSS and to allocate memory from the correct NUMA nodes
    is included in the application and is done transparently.
    The following table shows the mapping in this example:

    +----------+-----------+-----------+------------------------------------+
    | **Port** | **Queue** | **lcore** | **Description**                    |
    |          |           |           |                                    |
    +==========+===========+===========+====================================+
    | 0        | 0         | 0         | Map queue 0 from port 0 to lcore 0 |
    |          |           |           |                                    |
    +----------+-----------+-----------+------------------------------------+
    | 1        | 1         | 1         | Map queue 0 from port 1 to lcore 1 |
    |          |           |           |                                    |
    +----------+-----------+-----------+------------------------------------+

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The operation of this application is similar to that of the basic L3 Forwarding Sample Application.
See Section 10.4 "Explanation" for more information.
