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

Intel® QuickAssist Technology Sample Application
================================================

This sample application demonstrates the use of the cryptographic operations provided
by the Intel® QuickAssist Technology from within the DPDK environment.
Therefore, building and running this application requires having both the DPDK and
the QuickAssist Technology Software Library installed, as well as at least one
Intel® QuickAssist Technology hardware device present in the system.

For this sample application, there is a dependency on either of:

*   Intel® Communications Chipset 8900 to 8920 Series Software for Linux* package

*   Intel® Communications Chipset 8925 to 8955 Series Software for Linux* package

Overview
--------

An overview of the application is provided in :numref:`figure_quickassist_block_diagram`.
For simplicity, only two NIC ports and one Intel® QuickAssist Technology device are shown in this diagram,
although the number of NIC ports and Intel® QuickAssist Technology devices can be different.

.. _figure_quickassist_block_diagram:

.. figure:: img/quickassist_block_diagram.*

   Intel® QuickAssist Technology Application Block Diagram


The application allows the configuration of the following items:

*   Number of NIC ports

*   Number of logical cores (lcores)

*   Mapping of NIC RX queues to logical cores

Each lcore communicates with every cryptographic acceleration engine in the system through a pair of dedicated input - output queues.
Each lcore has a dedicated NIC TX queue with every NIC port in the system.
Therefore, each lcore reads packets from its NIC RX queues and cryptographic accelerator output queues and
writes packets to its NIC TX queues and cryptographic accelerator input queues.

Each incoming packet that is read from a NIC RX queue is either directly forwarded to its destination NIC TX port (forwarding path)
or first sent to one of the Intel® QuickAssist Technology devices for either encryption or decryption
before being sent out on its destination NIC TX port (cryptographic path).

The application supports IPv4 input packets only.
For each input packet, the decision between the forwarding path and
the cryptographic path is taken at the classification stage based on the value of the IP source address field read from the input packet.
Assuming that the IP source address is A.B.C.D, then if:

*   D = 0: the forwarding path is selected (the packet is forwarded out directly)

*   D = 1: the cryptographic path for encryption is selected (the packet is first encrypted and then forwarded out)

*   D = 2: the cryptographic path for decryption is selected (the packet is first decrypted and then forwarded out)

For the cryptographic path cases (D = 1 or D = 2), byte C specifies the cipher algorithm and
byte B the cryptographic hash algorithm to be used for the current packet.
Byte A is not used and can be any value.
The cipher and cryptographic hash algorithms supported by this application are listed in the crypto.h header file.

For each input packet, the destination NIC TX port is decided at the forwarding stage (executed after the cryptographic stage,
if enabled for the packet) by looking at the RX port index of the dst_ports[ ] array,
which was initialized at startup, being the outport the adjacent enabled port.
For example, if ports 1,3,5 and 6 are enabled, for input port 1, outport port will be 3 and vice versa,
and for input port 5, output port will be 6 and vice versa.

For the cryptographic path, it is the payload of the IPv4 packet that is encrypted or decrypted.

Setup
~~~~~

Building and running this application requires having both the DPDK package and
the QuickAssist Technology Software Library installed,
as well as at least one Intel® QuickAssist Technology hardware device present in the system.

For more details on how to build and run DPDK and Intel® QuickAssist Technology applications,
please refer to the following documents:

*   *DPDK Getting Started Guide*

*   Intel® Communications Chipset 8900 to 8920 Series Software for Linux* Getting Started Guide (440005)

*   Intel® Communications Chipset 8925 to 8955 Series Software for Linux* Getting Started Guide (523128)

For more details on the actual platforms used to validate this application, as well as performance numbers,
please refer to the Test Report, which is accessible by contacting your Intel representative.

Building the Application
------------------------

Steps to build the application:

#.  Set up the following environment variables:

    .. code-block:: console

        export RTE_SDK=<Absolute path to the DPDK installation folder>
        export ICP_ROOT=<Absolute path to the Intel QAT installation folder>

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    Refer to the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        cd ${RTE_SDK}/examples/dpdk_qat
        make

Running the Application
-----------------------

Intel® QuickAssist Technology Configuration Files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Intel® QuickAssist Technology configuration files used by the application are located in the config_files folder in the application folder.
There following sets of configuration files are included in the DPDK package:

*   Stargo CRB (single CPU socket): located in the stargo folder

    *   dh89xxcc_qa_dev0.conf

*   Shumway CRB (dual CPU socket): located in the shumway folder

    *   dh89xxcc_qa_dev0.conf

    *   dh89xxcc_qa_dev1.conf

*   Coleto Creek: located in the coleto folder

    *   dh895xcc_qa_dev0.conf

The relevant configuration file(s) must be copied to the /etc/ directory.

Please note that any change to these configuration files requires restarting the Intel®
QuickAssist Technology driver using the following command:

.. code-block:: console

    # service qat_service restart

Refer to the following documents for information on the Intel® QuickAssist Technology configuration files:

*   Intel®  Communications Chipset 8900 to 8920 Series Software Programmer's Guide

*   Intel®  Communications Chipset 8925 to 8955 Series Software Programmer's Guide

*   Intel®  Communications Chipset 8900 to 8920 Series Software for Linux* Getting Started Guide.

*   Intel®  Communications Chipset 8925 to 8955 Series Software for Linux* Getting Started Guide.

Traffic Generator Setup and Application Startup
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application has a number of command line options:

    dpdk_qat [EAL options] -- -p PORTMASK [--no-promisc] [--config '(port,queue,lcore)[,(port,queue,lcore)]']

where,

*   -p PORTMASK: Hexadecimal bitmask of ports to configure

*   --no-promisc: Disables promiscuous mode for all ports,
    so that only packets with the Ethernet MAC destination address set to the Ethernet address of the port are accepted.
    By default promiscuous mode is enabled so that packets are accepted regardless of the packet's Ethernet MAC destination address.

*   --config'(port,queue,lcore)[,(port,queue,lcore)]':  determines which queues from which ports are mapped to which cores.

Refer to Chapter 10 , "L3 Forwarding Sample Application" for more detailed descriptions of the --config command line option.

As an example, to run the application with two ports and two cores,
which are using different Intel® QuickAssist Technology execution engines,
performing AES-CBC-128 encryption with AES-XCBC-MAC-96 hash, the following settings can be used:

*   Traffic generator source IP address: 0.9.6.1

*   Command line:

    .. code-block:: console

        ./build/dpdk_qat -c 0xff -n 2 -- -p 0x3 --config '(0,0,1),(1,0,2)'

Refer to the *DPDK Test Report* for more examples of traffic generator setup and the application startup command lines.
If no errors are generated in response to the startup commands, the application is running correctly.
