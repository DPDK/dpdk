..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2017 Intel Corporation.

Introduction to the DPDK Sample Applications
============================================

The DPDK Sample Applications are small standalone applications which
demonstrate various features of DPDK. They can be considered as a cookbook of
DPDK features.  Users interested in getting started with DPDK can take the
applications, try out the features, and then extend them to fit their needs.


The DPDK Sample Applications
----------------------------

Table :numref:`table_sample_apps` shows a list of some of the main sample
applications that are available in the examples directory of DPDK:

 .. _table_sample_apps:

 .. table:: **Some of the DPDK Sample applications**

    +---------------------------------------+--------------------------------------+
    | Bonding                               | Netmap Compatibility                 |
    +---------------------------------------+--------------------------------------+
    | Command Line                          | Packet Ordering                      |
    +---------------------------------------+--------------------------------------+
    | Distributor                           | Performance Thread                   |
    +---------------------------------------+--------------------------------------+
    | Ethtool                               | Precision Time Protocol (PTP) Client |
    +---------------------------------------+--------------------------------------+
    | Exception Path                        | Quality of Service (QoS) Metering    |
    +---------------------------------------+--------------------------------------+
    | Hello World                           | QoS Scheduler                        |
    +---------------------------------------+--------------------------------------+
    | Internet Protocol (IP) Fragmentation  | Quota and Watermark                  |
    +---------------------------------------+--------------------------------------+
    | IP Pipeline                           | RX/TX Callbacks                      |
    +---------------------------------------+--------------------------------------+
    | IP Reassembly                         | Server node EFD                      |
    +---------------------------------------+--------------------------------------+
    | IPsec Security Gateway                | Basic Forwarding/Skeleton App        |
    +---------------------------------------+--------------------------------------+
    | IPv4 multicast                        | Tunnel End Point (TEP) termination   |
    +---------------------------------------+--------------------------------------+
    | Kernel NIC Interface                  | Timer                                |
    +---------------------------------------+--------------------------------------+
    | Network Layer 2 Forwarding + variants | Vhost                                |
    +---------------------------------------+--------------------------------------+
    | Network Layer 3 Forwarding + variants | Vhost Xen                            |
    +---------------------------------------+--------------------------------------+
    | Link Status Interrupt                 | VMDQ Forwarding                      |
    +---------------------------------------+--------------------------------------+
    | Load Balancer                         | VMDQ and DCB Forwarding              |
    +---------------------------------------+--------------------------------------+
    | Multi-process                         | VM Power Management                  |
    +---------------------------------------+--------------------------------------+

These examples range from simple to reasonably complex but most are designed
to demonstrate one particular feature of DPDK. Some of the more interesting
examples are highlighted below.


* :doc:`Hello World<hello_world>`: As with most introductions to a
  programming framework a good place to start is with the Hello World
  application. The Hello World example sets up the DPDK Environment Abstraction
  Layer (EAL), and prints a simple "Hello World" message to each of the DPDK
  enabled cores. This application doesn't do any packet forwarding but it is a
  good way to test if the DPDK environment is compiled and set up properly.

* :doc:`Basic Forwarding/Skeleton Application<skeleton>`: The Basic
  Forwarding/Skeleton contains the minimum amount of code required to enable
  basic packet forwarding with DPDK. This allows you to test if your network
  interfaces are working with DPDK.

* :doc:`Network Layer 2 forwarding<l2_forward_real_virtual>`: The Network Layer 2
  forwarding, or ``l2fwd`` application does forwarding based on Ethernet MAC
  addresses like a simple switch.

* :doc:`Network Layer 3 forwarding<l3_forward>`: The Network Layer3
  forwarding, or ``l3fwd`` application does forwarding based on Internet
  Protocol, IPv4 or IPv6 like a simple router.

* :doc:`Packet Distributor<dist_app>`: The Packet Distributor
  demonstrates how to distribute packets arriving on an Rx port to different
  cores for processing and transmission.

* :doc:`Multi-Process Application<multi_process>`: The
  multi-process application shows how two DPDK processes can work together using
  queues and memory pools to share information.

* :doc:`RX/TX callbacks Application<rxtx_callbacks>`: The RX/TX
  callbacks sample application is a packet forwarding application that
  demonstrates the use of user defined callbacks on received and transmitted
  packets. The application calculates the latency of a packet between RX
  (packet arrival) and TX (packet transmission) by adding callbacks to the RX
  and TX packet processing functions.

* :doc:`IPSec Security Gateway<ipsec_secgw>`: The IPSec Security
  Gateway application is minimal example of something closer to a real world
  example. This is also a good example of an application using the DPDK
  Cryptodev framework.

* :doc:`Precision Time Protocol (PTP) client<ptpclient>`: The PTP
  client is another minimal implementation of a real world application.
  In this case the application is a PTP client that communicates with a PTP
  master clock to synchronize time on a Network Interface Card (NIC) using the
  IEEE1588 protocol.

* :doc:`Quality of Service (QoS) Scheduler<qos_scheduler>`: The QoS
  Scheduler application demonstrates the use of DPDK to provide QoS scheduling.

There are many more examples shown in the following chapters. Each of the
documented sample applications show how to compile, configure and run the
application as well as explaining the main functionality of the code.
