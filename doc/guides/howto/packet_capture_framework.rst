..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

DPDK pdump Library and pdump Tool
=================================

This document describes how the Data Plane Development Kit (DPDK) Packet
Capture Framework is used for capturing packets on DPDK ports. It is intended
for users of DPDK who want to know more about the Packet Capture feature and
for those who want to monitor traffic on DPDK-controlled devices.

The DPDK packet capture framework was introduced in DPDK v16.07. The DPDK
packet capture framework consists of the DPDK pdump library and DPDK pdump
tool.


Introduction
------------

The :ref:`librte_pdump <pdump_library>` library provides the APIs required to
allow users to initialize the packet capture framework and to enable or
disable packet capture. The library works on a multi process communication model and its
usage is recommended for debugging purposes.

The :ref:`dpdk-pdump <pdump_tool>` tool is developed based on the
``librte_pdump`` library.  It runs as a DPDK secondary process and is capable
of enabling or disabling packet capture on DPDK ports. The ``dpdk-pdump`` tool
provides command-line options with which users can request enabling or
disabling of the packet capture on DPDK ports.

The application which initializes the packet capture framework will be a primary process
and the application that enables or disables the packet capture will
be a secondary process. The primary process sends the Rx and Tx packets from the DPDK ports
to the secondary process.

In DPDK the ``testpmd`` application can be used to initialize the packet
capture framework and acts as a server, and the ``dpdk-pdump`` tool acts as a
client. To view Rx or Tx packets of ``testpmd``, the application should be
launched first, and then the ``dpdk-pdump`` tool. Packets from ``testpmd``
will be sent to the tool, which then sends them on to the Pcap PMD device and
that device writes them to the Pcap file or to an external interface depending
on the command-line option used.

Some things to note:

* The ``dpdk-pdump`` tool can only be used in conjunction with a primary
  application which has the packet capture framework initialized already. In
  dpdk, only ``testpmd`` is modified to initialize packet capture framework,
  other applications remain untouched. So, if the ``dpdk-pdump`` tool has to
  be used with any application other than the testpmd, the user needs to
  explicitly modify that application to call the packet capture framework
  initialization code. Refer to the ``app/test-pmd/testpmd.c`` code and look
  for ``pdump`` keyword to see how this is done.

* The ``dpdk-pdump`` tool depends on the libpcap based PMD.


Test Environment
----------------

The overview of using the Packet Capture Framework and the ``dpdk-pdump`` tool
for packet capturing on the DPDK port in
:numref:`figure_packet_capture_framework`.

.. _figure_packet_capture_framework:

.. figure:: img/packet_capture_framework.*

   Packet capturing on a DPDK port using the dpdk-pdump tool.


Running the Application
-----------------------

The following steps demonstrate how to run the ``dpdk-pdump`` tool to capture
Rx side packets on dpdk_port0 in :numref:`figure_packet_capture_framework` and
inspect them using ``tcpdump``.

#. Launch testpmd as the primary application::

     sudo <build_dir>/app/dpdk-testpmd -c 0xf0 -n 4 -- -i --port-topology=chained

#. Launch the pdump tool as follows::

     sudo <build_dir>/app/dpdk-pdump -- \
          --pdump 'port=0,queue=*,rx-dev=/tmp/capture.pcap'

#. Send traffic to dpdk_port0 from traffic generator.
   Inspect packets captured in the file capture.pcap using a tool
   that can interpret Pcap files, for example tcpdump::

     $tcpdump -nr /tmp/capture.pcap
     reading from file /tmp/capture.pcap, link-type EN10MB (Ethernet)
     11:11:36.891404 IP 4.4.4.4.whois++ > 3.3.3.3.whois++: UDP, length 18
     11:11:36.891442 IP 4.4.4.4.whois++ > 3.3.3.3.whois++: UDP, length 18
     11:11:36.891445 IP 4.4.4.4.whois++ > 3.3.3.3.whois++: UDP, length 18
