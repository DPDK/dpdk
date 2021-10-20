..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2021 Intel Corporation.

DPDK packet capture libraries and tools
=======================================

This document describes how the Data Plane Development Kit (DPDK) Packet
Capture Framework is used for capturing packets on DPDK ports. It is intended
for users of DPDK who want to know more about the Packet Capture feature and
for those who want to monitor traffic on DPDK-controlled devices.

The DPDK packet capture framework was introduced in DPDK v16.07
and enhanced in 21.11.
The DPDK packet capture framework consists of the libraries
for collecting packets ``librte_pdump``
and writing packets to a file ``librte_pcapng``.
There are two sample applications: ``dpdk-dumpcap`` and older ``dpdk-pdump``.

Introduction
------------

The :doc:`librte_pdump <../prog_guide/pdump_lib>` library provides the API
required to allow users to initialize the packet capture framework
and to enable or disable packet capture.
The library works on a multi-process communication model
and its usage is recommended for debugging purposes.

The :doc:`librte_pcapng <../prog_guide/pcapng_lib>` library provides the API
to format packets and write them to a file in Pcapng format.

The :doc:`dpdk-dumpcap <../tools/dumpcap>` is a tool that captures packets in
like Wireshark dumpcap does for Linux.
It runs as a DPDK secondary process and captures packets
from one or more interfaces and writes them to a file in Pcapng format.
The ``dpdk-dumpcap`` tool is designed to take
most of the same options as the Wireshark ``dumpcap`` command.

Without any options it will use the packet capture framework
to capture traffic from the first available DPDK port.

The ``dpdk-testpmd`` application can be used to initialize
the packet capture framework and acts as a server,
and the ``dpdk-dumpcap`` tool acts as a client.
To view Rx or Tx packets of ``dpdk-testpmd``,
the application should be launched first,
and then the ``dpdk-dumpcap`` tool.
Packets from ``dpdk-testpmd`` will be sent to the tool,
and then to the Pcapng file.

Some things to note:

* All tools using ``librte_pdump`` can only be used in conjunction with a primary
  application which has the packet capture framework initialized already. In
  dpdk, only ``testpmd`` is modified to initialize packet capture framework,
  other applications remain untouched. So, if the ``dpdk-dumpcap`` tool has to
  be used with any application other than the testpmd, the user needs to
  explicitly modify that application to call the packet capture framework
  initialization code. Refer to the ``app/test-pmd/testpmd.c`` code and look
  for ``pdump`` keyword to see how this is done.

* The ``dpdk-pdump`` tool is an older tool
  created as demonstration of ``librte_pdump`` library.
  The ``dpdk-pdump`` tool provides more limited functionality
  and depends on the Pcap PMD.
  It is retained only for compatibility reasons;
  users should use ``dpdk-dumpcap`` instead.


Test Environment
----------------

The overview of using the Packet Capture Framework and the ``dpdk-dumpcap`` utility
for packet capturing on the DPDK port in
:numref:`figure_packet_capture_framework`.

.. _figure_packet_capture_framework:

.. figure:: img/packet_capture_framework.*

   Packet capturing on a DPDK port using the dpdk-dumpcap utility.


Running the Application
-----------------------

The following steps demonstrate how to run the ``dpdk-dumpcap`` tool to capture
Rx side packets on dpdk_port0 in :numref:`figure_packet_capture_framework` and
inspect them using ``tcpdump``.

#. Launch testpmd as the primary application::

     sudo <build_dir>/app/dpdk-testpmd -c 0xf0 -n 4 -- -i --port-topology=chained

#. Launch the dpdk-dumpcap as follows::

     sudo <build_dir>/app/dpdk-dumpcap -w /tmp/capture.pcapng

#. Send traffic to dpdk_port0 from traffic generator.
   Inspect packets captured in the file capture.pcapng using a tool
   such as tcpdump or tshark that can interpret Pcapng files::

     $ tcpdump -nr /tmp/capture.pcapng
     reading from file /tmp/capture.pcap, link-type EN10MB (Ethernet)
     11:11:36.891404 IP 4.4.4.4.whois++ > 3.3.3.3.whois++: UDP, length 18
     11:11:36.891442 IP 4.4.4.4.whois++ > 3.3.3.3.whois++: UDP, length 18
     11:11:36.891445 IP 4.4.4.4.whois++ > 3.3.3.3.whois++: UDP, length 18
