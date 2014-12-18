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

Internet Protocol (IP) Pipeline Sample Application
==================================================

The Internet Protocol (IP) Pipeline application illustrates the use of the DPDK Packet Framework tool suite.
The DPDK pipeline methodology is used to implement functional blocks such as
packet RX, packet TX, flow classification, firewall,
routing, IP fragmentation, IP reassembly, etc
which are then assigned to different CPU cores and connected together to create complex multi-core applications.

Overview
--------

The pipelines for packet RX, packet TX, flow classification, firewall, routing, IP fragmentation, IP reassembly, management, etc
are instantiated and different CPU cores and connected together through software queues.
One of the CPU cores can be designated as the management core to run a Command Line Interface (CLI) to add entries to each table
(e.g. flow table, firewall rule database, routing table, Address Resolution Protocol (ARP) table, and so on),
bring NIC ports up or down, and so on.

Compiling the Application
-------------------------

#.  Go to the examples directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/ip_pipeline

#.  Set the target (a default target is used if not specified):

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

#.  Build the application:

    .. code-block:: console

        make

Running the Sample Code
-----------------------

The application execution command line is:

.. code-block:: console

    ./ip_pipeline [EAL options] -- -p PORTMASK [-f CONFIG_FILE]

The number of ports in the PORTMASK can be either 2 or 4.

The config file assigns functionality to the CPU core by deciding the pipeline type to run on each CPU core
(e.g. master, RX, flow classification, firewall, routing, IP fragmentation, IP reassembly, TX) and
also allows creating complex topologies made up of CPU cores by interconnecting the CPU cores through SW queues.

Once the application is initialized, the CLI is available for populating the application tables,
bringing NIC ports up or down, and so on.

The flow classification pipeline implements the flow table by using a large (multi-million entry) hash table with a 16-byte key size.
The lookup key is the IPv4 5-tuple, which is extracted from the input packet by the packet RX pipeline and
saved in the packet meta-data, has the following format:

    [source IP address, destination IP address, L4 protocol, L4 protocol source port, L4 protocol destination port]

The firewall pipeline implements the rule database using an ACL table.

The routing pipeline implements an IP routing table by using an LPM IPv4 table and
an ARP table by using a hash table with an 8-byte key size.
The IP routing table lookup provides the output interface ID and the next hop IP address,
which are stored in the packet meta-data, then used as the lookup key into the ARP table.
The ARP table lookup provides the destination MAC address to be used for the output packet.
The action for the default entry of both the IP routing table and the ARP table is packet drop.

The following CLI operations are available:

*   Enable/disable NIC ports (RX pipeline)

*   Add/delete/list flows (flow classification pipeline)

*   Add/delete/list firewall rules (firewall pipeline)

*   Add/delete/list routes (routing pipeline)

*   Add/delete/list ARP entries (routing pipeline)

In addition, there are two special commands:

*   flow add all:
    Populate the flow classification table with 16 million flows
    (by iterating through the last three bytes of the destination IP address).
    These flows are not displayed when using the flow print command.
    When this command is used, the following traffic profile must be used to have flow table lookup hits for all input packets.
    TCP/IPv4 packets with:

    *   destination IP address = A.B.C.D with A fixed to 0 and B,C,D random

    *   source IP address fixed to 0

    *   source TCP port fixed to 0

    *   destination TCP port fixed to 0

*   run cmd_file_path: Read CLI commands from an external file and run them one by one.

The full list of the available CLI commands can be displayed by pressing the TAB key while the application is running.
