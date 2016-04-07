..  BSD LICENSE
    Copyright(c) 2016 Intel Corporation. All rights reserved.
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

IPsec Security Gateway Sample Application
=========================================

The IPsec Security Gateway application is an example of a "real world"
application using DPDK cryptodev framework.

Overview
--------

The application demonstrates the implementation of a Security Gateway
(not IPsec compliant, see Constraints bellow) using DPDK based on RFC4301,
RFC4303, RFC3602 and RFC2404.

Internet Key Exchange (IKE) is not implemented, so only manual setting of
Security Policies and Security Associations is supported.

The Security Policies (SP) are implemented as ACL rules, the Security
Associations (SA) are stored in a table and the Routing is implemented
using LPM.

The application classify the ports between Protected and Unprotected.
Thus, traffic received in an Unprotected or Protected port is consider
Inbound or Outbound respectively.

Path for IPsec Inbound traffic:

*  Read packets from the port
*  Classify packets between IPv4 and ESP.
*  Inbound SA lookup for ESP packets based on their SPI
*  Verification/Decryption
*  Removal of ESP and outer IP header
*  Inbound SP check using ACL of decrypted packets and any other IPv4 packet
   we read.
*  Routing
*  Write packet to port

Path for IPsec Outbound traffic:

*  Read packets from the port
*  Outbound SP check using ACL of all IPv4 traffic
*  Outbound SA lookup for packets that need IPsec protection
*  Add ESP and outer IP header
*  Encryption/Digest
*  Routing
*  Write packet to port

Constraints
-----------
*  IPv4 traffic
*  ESP tunnel mode
*  EAS-CBC, HMAC-SHA1 and NULL
*  Each SA must be handle by a unique lcore (1 RX queue per port)
*  No chained mbufs

Compiling the Application
-------------------------

To compile the application:

#. Go to the sample application directory:

   .. code-block:: console

      export RTE_SDK=/path/to/rte_sdk
      cd ${RTE_SDK}/examples/ipsec-secgw

#. Set the target (a default target is used if not specified). For example:

   .. code-block:: console

      export RTE_TARGET=x86_64-native-linuxapp-gcc

   See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#. Build the application:

   .. code-block:: console

       make

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

   ./build/ipsec-secgw [EAL options] -- -p PORTMASK -P -u PORTMASK --config
   (port,queue,lcore)[,(port,queue,lcore] --single-sa SAIDX --ep0|--ep1

where,

*   -p PORTMASK: Hexadecimal bitmask of ports to configure

*   -P: optional, sets all ports to promiscuous mode so that packets are
    accepted regardless of the packet's Ethernet MAC destination address.
    Without this option, only packets with the Ethernet MAC destination address
    set to the Ethernet address of the port are accepted (default is enabled).

*   -u PORTMASK: hexadecimal bitmask of unprotected ports

*   --config (port,queue,lcore)[,(port,queue,lcore)]: determines which queues
    from which ports are mapped to which cores

*   --single-sa SAIDX: use a single SA for outbound traffic, bypassing the SP
    on both Inbound and Outbound. This option is meant for debugging/performance
    purposes.

*   --ep0: configure the app as Endpoint 0.

*   --ep1: configure the app as Endpoint 1.

Either one of --ep0 or --ep1 *must* be specified.
The main purpose of these options is two easily configure two systems
back-to-back that would forward traffic through an IPsec tunnel.

The mapping of lcores to port/queues is similar to other l3fwd applications.

For example, given the following command line:

.. code-block:: console

    ./build/ipsec-secgw -l 20,21 -n 4 --socket-mem 0,2048
           --vdev "cryptodev_null_pmd" -- -p 0xf -P -u 0x3
           --config="(0,0,20),(1,0,20),(2,0,21),(3,0,21)" --ep0

where each options means:

*   The -l option enables cores 20 and 21

*   The -n option sets memory 4 channels

*   The --socket-mem to use 2GB on socket 1

*   The --vdev "cryptodev_null_pmd" option creates virtual NULL cryptodev PMD

*   The -p option enables ports (detected) 0, 1, 2 and 3

*   The -P option enables promiscuous mode

*   The -u option sets ports 1 and 2 as unprotected, leaving 2 and 3 as protected

*   The --config option enables one queue per port with the following mapping:

+----------+-----------+-----------+---------------------------------------+
| **Port** | **Queue** | **lcore** | **Description**                       |
|          |           |           |                                       |
+----------+-----------+-----------+---------------------------------------+
| 0        | 0         | 20        | Map queue 0 from port 0 to lcore 20.  |
|          |           |           |                                       |
+----------+-----------+-----------+---------------------------------------+
| 1        | 0         | 20        | Map queue 0 from port 1 to lcore 20.  |
|          |           |           |                                       |
+----------+-----------+-----------+---------------------------------------+
| 2        | 0         | 21        | Map queue 0 from port 2 to lcore 21.  |
|          |           |           |                                       |
+----------+-----------+-----------+---------------------------------------+
| 3        | 0         | 21        | Map queue 0 from port 3 to lcore 21.  |
|          |           |           |                                       |
+----------+-----------+-----------+---------------------------------------+

*   The --ep0 options configures the app with a given set of SP, SA and Routing
    entries as explained below in more detail.

Refer to the *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.

The application would do a best effort to "map" crypto devices to cores, with
hardware devices having priority.
This means that if the application is using a single core and both hardware
and software crypto devices are detected, hardware devices will be used.

A way to achieve the case where you want to force the use of virtual crypto
devices is to whitelist the Ethernet devices needed and therefore implicitly
blacklisting all hardware crypto devices.

For example, something like the following command line:

.. code-block:: console

    ./build/ipsec-secgw -l 20,21 -n 4 --socket-mem 0,2048
            -w 81:00.0 -w 81:00.1 -w 81:00.2 -w 81:00.3
            --vdev "cryptodev_aesni_mb_pmd" --vdev "cryptodev_null_pmd" --
            -p 0xf -P -u 0x3 --config="(0,0,20),(1,0,20),(2,0,21),(3,0,21)"
            --ep0

Configurations
--------------

The following sections provide some details on the default values used to
initialize the SP, SA and Routing tables.
Currently all the configuration is hard coded into the application.

Security Policy Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As mention in the overview, the Security Policies are ACL rules.
The application defines two ACLs, one each of Inbound and Outbound, and
it replicates them per socket in use.

Following are the default rules:

Endpoint 0 Outbound Security Policies:

+---------+------------------+-----------+------------+
| **Src** | **Dst**          | **proto** | **SA idx** |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.105.0/24 | Any       | 5          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.106.0/24 | Any       | 6          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.107.0/24 | Any       | 7          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.108.0/24 | Any       | 8          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.200.0/24 | Any       | 9          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.250.0/24 | Any       | BYPASS     |
|         |                  |           |            |
+---------+------------------+-----------+------------+

Endpoint 0 Inbound Security Policies:

+---------+------------------+-----------+------------+
| **Src** | **Dst**          | **proto** | **SA idx** |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.115.0/24 | Any       | 5          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.116.0/24 | Any       | 6          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.117.0/24 | Any       | 7          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.118.0/24 | Any       | 8          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.210.0/24 | Any       | 9          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.240.0/24 | Any       | BYPASS     |
|         |                  |           |            |
+---------+------------------+-----------+------------+

Endpoint 1 Outbound Security Policies:

+---------+------------------+-----------+------------+
| **Src** | **Dst**          | **proto** | **SA idx** |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.115.0/24 | Any       | 5          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.116.0/24 | Any       | 6          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.117.0/24 | Any       | 7          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.118.0/24 | Any       | 8          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.210.0/24 | Any       | 9          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.240.0/24 | Any       | BYPASS     |
|         |                  |           |            |
+---------+------------------+-----------+------------+

Endpoint 1 Inbound Security Policies:

+---------+------------------+-----------+------------+
| **Src** | **Dst**          | **proto** | **SA idx** |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.105.0/24 | Any       | 5          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.106.0/24 | Any       | 6          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.107.0/24 | Any       | 7          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.108.0/24 | Any       | 8          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.200.0/24 | Any       | 9          |
|         |                  |           |            |
+---------+------------------+-----------+------------+
| Any     | 192.168.250.0/24 | Any       | BYPASS     |
|         |                  |           |            |
+---------+------------------+-----------+------------+


Security Association Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The SAs are kept in a array table.

For Inbound, the SPI is used as index module the table size.
This means that on a table for 100 SA, SPI 5 and 105 would use the same index
and that is not currently supported.

Notice that it is not an issue for Outbound traffic as we store the index and
not the SPI in the Security Policy.

All SAs configured with AES-CBC and HMAC-SHA1 share the same values for cipher
block size and key, and authentication digest size and key.

Following are the default values:

Endpoint 0 Outbound Security Associations:

+---------+------------+-----------+----------------+------------------+
| **SPI** | **Cipher** | **Auth**  | **Tunnel src** | **Tunnel dst**   |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 5       | AES-CBC    | HMAC-SHA1 | 172.16.1.5     | 172.16.2.5       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 6       | AES-CBC    | HMAC-SHA1 | 172.16.1.6     | 172.16.2.6       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 7       | AES-CBC    | HMAC-SHA1 | 172.16.1.7     | 172.16.2.7       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 8       | AES-CBC    | HMAC-SHA1 | 172.16.1.8     | 172.16.2.8       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 9       | NULL       | NULL      | 172.16.1.5     | 172.16.2.5       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+

Endpoint 0 Inbound Security Associations:

+---------+------------+-----------+----------------+------------------+
| **SPI** | **Cipher** | **Auth**  | **Tunnel src** | **Tunnel dst**   |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 5       | AES-CBC    | HMAC-SHA1 | 172.16.2.5     | 172.16.1.5       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 6       | AES-CBC    | HMAC-SHA1 | 172.16.2.6     | 172.16.1.6       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 7       | AES-CBC    | HMAC-SHA1 | 172.16.2.7     | 172.16.1.7       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 8       | AES-CBC    | HMAC-SHA1 | 172.16.2.8     | 172.16.1.8       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 9       | NULL       | NULL      | 172.16.2.5     | 172.16.1.5       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+

Endpoint 1 Outbound Security Associations:

+---------+------------+-----------+----------------+------------------+
| **SPI** | **Cipher** | **Auth**  | **Tunnel src** | **Tunnel dst**   |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 5       | AES-CBC    | HMAC-SHA1 | 172.16.2.5     | 172.16.1.5       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 6       | AES-CBC    | HMAC-SHA1 | 172.16.2.6     | 172.16.1.6       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 7       | AES-CBC    | HMAC-SHA1 | 172.16.2.7     | 172.16.1.7       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 8       | AES-CBC    | HMAC-SHA1 | 172.16.2.8     | 172.16.1.8       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 9       | NULL       | NULL      | 172.16.2.5     | 172.16.1.5       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+

Endpoint 1 Inbound Security Associations:

+---------+------------+-----------+----------------+------------------+
| **SPI** | **Cipher** | **Auth**  | **Tunnel src** | **Tunnel dst**   |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 5       | AES-CBC    | HMAC-SHA1 | 172.16.1.5     | 172.16.2.5       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 6       | AES-CBC    | HMAC-SHA1 | 172.16.1.6     | 172.16.2.6       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 7       | AES-CBC    | HMAC-SHA1 | 172.16.1.7     | 172.16.2.7       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 8       | AES-CBC    | HMAC-SHA1 | 172.16.1.8     | 172.16.2.8       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+
| 9       | NULL       | NULL      | 172.16.1.5     | 172.16.2.5       |
|         |            |           |                |                  |
+---------+------------+-----------+----------------+------------------+

Routing Initialization
~~~~~~~~~~~~~~~~~~~~~~

The Routing is implemented using LPM table.

Following default values:

Endpoint 0 Routing Table:

+------------------+----------+
| **Dst addr**     | **Port** |
|                  |          |
+------------------+----------+
| 172.16.2.5/32    | 0        |
|                  |          |
+------------------+----------+
| 172.16.2.6/32    | 0        |
|                  |          |
+------------------+----------+
| 172.16.2.7/32    | 1        |
|                  |          |
+------------------+----------+
| 172.16.2.8/32    | 1        |
|                  |          |
+------------------+----------+
| 192.168.115.0/24 | 2        |
|                  |          |
+------------------+----------+
| 192.168.116.0/24 | 2        |
|                  |          |
+------------------+----------+
| 192.168.117.0/24 | 3        |
|                  |          |
+------------------+----------+
| 192.168.118.0/24 | 3        |
|                  |          |
+------------------+----------+
| 192.168.210.0/24 | 2        |
|                  |          |
+------------------+----------+
| 192.168.240.0/24 | 2        |
|                  |          |
+------------------+----------+
| 192.168.250.0/24 | 0        |
|                  |          |
+------------------+----------+

Endpoint 1 Routing Table:

+------------------+----------+
| **Dst addr**     | **Port** |
|                  |          |
+------------------+----------+
| 172.16.1.5/32    | 2        |
|                  |          |
+------------------+----------+
| 172.16.1.6/32    | 2        |
|                  |          |
+------------------+----------+
| 172.16.1.7/32    | 3        |
|                  |          |
+------------------+----------+
| 172.16.1.8/32    | 3        |
|                  |          |
+------------------+----------+
| 192.168.105.0/24 | 0        |
|                  |          |
+------------------+----------+
| 192.168.106.0/24 | 0        |
|                  |          |
+------------------+----------+
| 192.168.107.0/24 | 1        |
|                  |          |
+------------------+----------+
| 192.168.108.0/24 | 1        |
|                  |          |
+------------------+----------+
| 192.168.200.0/24 | 0        |
|                  |          |
+------------------+----------+
| 192.168.240.0/24 | 2        |
|                  |          |
+------------------+----------+
| 192.168.250.0/24 | 0        |
|                  |          |
+------------------+----------+
