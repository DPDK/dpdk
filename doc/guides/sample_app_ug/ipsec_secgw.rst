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
(not IPsec compliant, see the Constraints section below) using DPDK based on RFC4301,
RFC4303, RFC3602 and RFC2404.

Internet Key Exchange (IKE) is not implemented, so only manual setting of
Security Policies and Security Associations is supported.

The Security Policies (SP) are implemented as ACL rules, the Security
Associations (SA) are stored in a table and the routing is implemented
using LPM.

The application classifies the ports as *Protected* and *Unprotected*.
Thus, traffic received on an Unprotected or Protected port is consider
Inbound or Outbound respectively.

The Path for IPsec Inbound traffic is:

*  Read packets from the port.
*  Classify packets between IPv4 and ESP.
*  Perform Inbound SA lookup for ESP packets based on their SPI.
*  Perform Verification/Decryption.
*  Remove ESP and outer IP header
*  Inbound SP check using ACL of decrypted packets and any other IPv4 packets.
*  Routing.
*  Write packet to port.

The Path for the IPsec Outbound traffic is:

*  Read packets from the port.
*  Perform Outbound SP check using ACL of all IPv4 traffic.
*  Perform Outbound SA lookup for packets that need IPsec protection.
*  Add ESP and outer IP header.
*  Perform Encryption/Digest.
*  Routing.
*  Write packet to port.


Constraints
-----------

*  No IPv6 options headers.
*  No AH mode.
*  Currently only EAS-CBC, HMAC-SHA1 and NULL.
*  Each SA must be handle by a unique lcore (*1 RX queue per port*).
*  No chained mbufs.


Compiling the Application
-------------------------

To compile the application:

#. Go to the sample application directory::

      export RTE_SDK=/path/to/rte_sdk
      cd ${RTE_SDK}/examples/ipsec-secgw

#. Set the target (a default target is used if not specified). For example::


      export RTE_TARGET=x86_64-native-linuxapp-gcc

   See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#. Build the application::

       make

#. [Optional] Build the application for debugging:
   This option adds some extra flags, disables compiler optimizations and
   is verbose::

       make DEBUG=1


Running the Application
-----------------------

The application has a number of command line options::


   ./build/ipsec-secgw [EAL options] --
                        -p PORTMASK -P -u PORTMASK
                        --config (port,queue,lcore)[,(port,queue,lcore]
                        --single-sa SAIDX
			--ep0|--ep1

Where:

*   ``-p PORTMASK``: Hexadecimal bitmask of ports to configure.

*   ``-P``: *optional*. Sets all ports to promiscuous mode so that packets are
    accepted regardless of the packet's Ethernet MAC destination address.
    Without this option, only packets with the Ethernet MAC destination address
    set to the Ethernet address of the port are accepted (default is enabled).

*   ``-u PORTMASK``: hexadecimal bitmask of unprotected ports

*   ``--config (port,queue,lcore)[,(port,queue,lcore)]``: determines which queues
    from which ports are mapped to which cores.

*   ``--single-sa SAIDX``: use a single SA for outbound traffic, bypassing the SP
    on both Inbound and Outbound. This option is meant for debugging/performance
    purposes.

*   ``--ep0``: configure the app as Endpoint 0.

*   ``--ep1``: configure the app as Endpoint 1.

Either one of ``--ep0`` or ``--ep1`` **must** be specified.
The main purpose of these options is to easily configure two systems
back-to-back that would forward traffic through an IPsec tunnel (see
:ref:`figure_ipsec_endpoints`).

The mapping of lcores to port/queues is similar to other l3fwd applications.

For example, given the following command line::

    ./build/ipsec-secgw -l 20,21 -n 4 --socket-mem 0,2048       \
           --vdev "cryptodev_null_pmd" -- -p 0xf -P -u 0x3      \
           --config="(0,0,20),(1,0,20),(2,0,21),(3,0,21)" --ep0 \

where each options means:

*   The ``-l`` option enables cores 20 and 21.

*   The ``-n`` option sets memory 4 channels.

*   The ``--socket-mem`` to use 2GB on socket 1.

*   The ``--vdev "cryptodev_null_pmd"`` option creates virtual NULL cryptodev PMD.

*   The ``-p`` option enables ports (detected) 0, 1, 2 and 3.

*   The ``-P`` option enables promiscuous mode.

*   The ``-u`` option sets ports 1 and 2 as unprotected, leaving 2 and 3 as protected.

*   The ``--config`` option enables one queue per port with the following mapping:

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

*   The ``--ep0`` options configures the app with a given set of SP, SA and Routing
    entries as explained below in more detail.

Refer to the *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.

The application would do a best effort to "map" crypto devices to cores, with
hardware devices having priority. Basically, hardware devices if present would
be assigned to a core before software ones.
This means that if the application is using a single core and both hardware
and software crypto devices are detected, hardware devices will be used.

A way to achieve the case where you want to force the use of virtual crypto
devices is to whitelist the Ethernet devices needed and therefore implicitly
blacklisting all hardware crypto devices.

For example, something like the following command line:

.. code-block:: console

    ./build/ipsec-secgw -l 20,21 -n 4 --socket-mem 0,2048 \
            -w 81:00.0 -w 81:00.1 -w 81:00.2 -w 81:00.3 \
            --vdev "cryptodev_aesni_mb_pmd" --vdev "cryptodev_null_pmd" \
	    -- \
            -p 0xf -P -u 0x3 --config="(0,0,20),(1,0,20),(2,0,21),(3,0,21)" \
            --ep0


Configurations
--------------

The following sections provide some details on the default values used to
initialize the SP, SA and Routing tables.
Currently all configuration information is hard coded into the application.

The following image illustrate a few of the concepts regarding IPSec, such
as protected/unprotected and inbound/outbound traffic, from the point of
view of two back-to-back endpoints:

.. _figure_ipsec_endpoints:

.. figure:: img/ipsec_endpoints.*

   IPSec Inbound/Outbound traffic

Note that the above image only displays unidirectional traffic per port
for illustration purposes.
The application supports bidirectional traffic on all ports,


Security Policy Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As mention in the overview, the Security Policies are ACL rules.
The application defines two ACLs, one each of Inbound and Outbound, and
it replicates them per socket in use.

Following are the default rules which show only the relevant information,
assuming ANY value is valid for the fields not mentioned (src ip, proto,
src/dst ports).

.. _table_ipsec_endpoint_outbound_sp:

.. table:: Endpoint 0 Outbound Security Policies

   +-----------------------------------+------------+
   | **Dst**                           | **SA idx** |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.105.0/24                  | 5          |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.106.0/24                  | 6          |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.175.0/24                  | 10         |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.176.0/24                  | 11         |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.200.0/24                  | 15         |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.201.0/24                  | 16         |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.55.0/24                   | 25         |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.56.0/24                   | 26         |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.240.0/24                  | BYPASS     |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.241.0/24                  | BYPASS     |
   |                                   |            |
   +-----------------------------------+------------+
   | 0:0:0:0:5555:5555:0:0/96          | 5          |
   |                                   |            |
   +-----------------------------------+------------+
   | 0:0:0:0:6666:6666:0:0/96          | 6          |
   |                                   |            |
   +-----------------------------------+------------+
   | 0:0:1111:1111:0:0:0:0/96          | 10         |
   |                                   |            |
   +-----------------------------------+------------+
   | 0:0:1111:1111:1111:1111:0:0/96    | 11         |
   |                                   |            |
   +-----------------------------------+------------+
   | 0:0:0:0:aaaa:aaaa:0:0/96          | 25         |
   |                                   |            |
   +-----------------------------------+------------+
   | 0:0:0:0:bbbb:bbbb:0:0/96          | 26         |
   |                                   |            |
   +-----------------------------------+------------+

.. _table_ipsec_endpoint_inbound_sp:

.. table:: Endpoint 0 Inbound Security Policies

   +-----------------------------------+------------+
   | **Dst**                           | **SA idx** |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.115.0/24                  | 105        |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.116.0/24                  | 106        |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.185.0/24                  | 110        |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.186.0/24                  | 111        |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.210.0/24                  | 115        |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.211.0/24                  | 116        |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.65.0/24                   | 125        |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.66.0/24                   | 126        |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.245.0/24                  | BYPASS     |
   |                                   |            |
   +-----------------------------------+------------+
   | 192.168.246.0/24                  | BYPASS     |
   |                                   |            |
   +-----------------------------------+------------+
   | ffff:0:0:0:5555:5555:0:0/96       | 105        |
   |                                   |            |
   +-----------------------------------+------------+
   | ffff:0:0:0:6666:6666:0:0/96       | 106        |
   |                                   |            |
   +-----------------------------------+------------+
   | ffff:0:1111:1111:0:0:0:0/96       | 110        |
   |                                   |            |
   +-----------------------------------+------------+
   | ffff:0:1111:1111:1111:1111:0:0/96 | 111        |
   |                                   |            |
   +-----------------------------------+------------+
   | ffff:0:0:0:aaaa:aaaa:0:0/96       | 125        |
   |                                   |            |
   +-----------------------------------+------------+
   | ffff:0:0:0:bbbb:bbbb:0:0/96       | 126        |
   |                                   |            |
   +-----------------------------------+------------+

For Endpoint 1, we use the same policies in reverse, meaning the Inbound SP
entries are set as Outbound and vice versa.


Security Association Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The SAs are kept in a array table.

For Inbound, the SPI is used as index modulo the table size.
This means that on a table for 100 SA, SPI 5 and 105 would use the same index
and that is not currently supported.

Notice that it is not an issue for Outbound traffic as we store the index and
not the SPI in the Security Policy.

All SAs configured with AES-CBC and HMAC-SHA1 share the same values for cipher
block size and key, and authentication digest size and key.

The following are the default values:

.. _table_ipsec_endpoint_outbound_sa:

.. table:: Endpoint 0 Outbound Security Associations

   +---------+----------+------------+-----------+----------------+----------------+
   | **SPI** | **Mode** | **Cipher** | **Auth**  | **Tunnel src** | **Tunnel dst** |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 5       | Tunnel   | AES-CBC    | HMAC-SHA1 | 172.16.1.5     | 172.16.2.5     |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 6       | Tunnel   | AES-CBC    | HMAC-SHA1 | 172.16.1.6     | 172.16.2.6     |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 10      | Trans    | AES-CBC    | HMAC-SHA1 | N/A            | N/A            |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 11      | Trans    | AES-CBC    | HMAC-SHA1 | N/A            | N/A            |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 15      | Tunnel   | NULL       | NULL      | 172.16.1.5     | 172.16.2.5     |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 16      | Tunnel   | NULL       | NULL      | 172.16.1.6     | 172.16.2.6     |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 25      | Tunnel   | AES-CBC    | HMAC-SHA1 | 1111:1111:     | 2222:2222:     |
   |         |          |            |           | 1111:1111:     | 2222:2222:     |
   |         |          |            |           | 1111:1111:     | 2222:2222:     |
   |         |          |            |           | 1111:5555      | 2222:5555      |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 26      | Tunnel   | AES-CBC    | HMAC-SHA1 | 1111:1111:     | 2222:2222:     |
   |         |          |            |           | 1111:1111:     | 2222:2222:     |
   |         |          |            |           | 1111:1111:     | 2222:2222:     |
   |         |          |            |           | 1111:6666      | 2222:6666      |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+

.. _table_ipsec_endpoint_inbound_sa:

.. table:: Endpoint 0 Inbound Security Associations

   +---------+----------+------------+-----------+----------------+----------------+
   | **SPI** | **Mode** | **Cipher** | **Auth**  | **Tunnel src** | **Tunnel dst** |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 105     | Tunnel   | AES-CBC    | HMAC-SHA1 | 172.16.2.5     | 172.16.1.5     |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 106     | Tunnel   | AES-CBC    | HMAC-SHA1 | 172.16.2.6     | 172.16.1.6     |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 110     | Trans    | AES-CBC    | HMAC-SHA1 | N/A            | N/A            |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 111     | Trans    | AES-CBC    | HMAC-SHA1 | N/A            | N/A            |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 115     | Tunnel   | NULL       | NULL      | 172.16.2.5     | 172.16.1.5     |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 116     | Tunnel   | NULL       | NULL      | 172.16.2.6     | 172.16.1.6     |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 125     | Tunnel   | AES-CBC    | HMAC-SHA1 | 2222:2222:     | 1111:1111:     |
   |         |          |            |           | 2222:2222:     | 1111:1111:     |
   |         |          |            |           | 2222:2222:     | 1111:1111:     |
   |         |          |            |           | 2222:5555      | 1111:5555      |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+
   | 126     | Tunnel   | AES-CBC    | HMAC-SHA1 | 2222:2222:     | 1111:1111:     |
   |         |          |            |           | 2222:2222:     | 1111:1111:     |
   |         |          |            |           | 2222:2222:     | 1111:1111:     |
   |         |          |            |           | 2222:6666      | 1111:6666      |
   |         |          |            |           |                |                |
   +---------+----------+------------+-----------+----------------+----------------+

For Endpoint 1, we use the same policies in reverse, meaning the Inbound SP
entries are set as Outbound and vice versa.


Routing Initialization
~~~~~~~~~~~~~~~~~~~~~~

The Routing is implemented using an LPM table.

Following default values:

.. _table_ipsec_endpoint_outbound_routing:

.. table:: Endpoint 0 Routing Table

   +------------------+----------+
   | **Dst addr**     | **Port** |
   |                  |          |
   +------------------+----------+
   | 172.16.2.5/32    | 0        |
   |                  |          |
   +------------------+----------+
   | 172.16.2.6/32    | 1        |
   |                  |          |
   +------------------+----------+
   | 192.168.175.0/24 | 0        |
   |                  |          |
   +------------------+----------+
   | 192.168.176.0/24 | 1        |
   |                  |          |
   +------------------+----------+
   | 192.168.240.0/24 | 0        |
   |                  |          |
   +------------------+----------+
   | 192.168.241.0/24 | 1        |
   |                  |          |
   +------------------+----------+
   | 192.168.115.0/24 | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.116.0/24 | 3        |
   |                  |          |
   +------------------+----------+
   | 192.168.65.0/24  | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.66.0/24  | 3        |
   |                  |          |
   +------------------+----------+
   | 192.168.185.0/24 | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.186.0/24 | 3        |
   |                  |          |
   +------------------+----------+
   | 192.168.210.0/24 | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.211.0/24 | 3        |
   |                  |          |
   +------------------+----------+
   | 192.168.245.0/24 | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.246.0/24 | 3        |
   |                  |          |
   +------------------+----------+
   | 2222:2222:       | 0        |
   | 2222:2222:       |          |
   | 2222:2222:       |          |
   | 2222:5555/116    |          |
   |                  |          |
   +------------------+----------+
   | 2222:2222:       | 1        |
   | 2222:2222:       |          |
   | 2222:2222:       |          |
   | 2222:6666/116    |          |
   |                  |          |
   +------------------+----------+
   | 0000:0000:       | 0        |
   | 1111:1111:       |          |
   | 0000:0000:       |          |
   | 0000:0000/116    |          |
   |                  |          |
   +------------------+----------+
   | 0000:0000:       | 1        |
   | 1111:1111:       |          |
   | 1111:1111:       |          |
   | 0000:0000/116    |          |
   |                  |          |
   +------------------+----------+
   | ffff:0000:       | 2        |
   | 0000:0000:       |          |
   | aaaa:aaaa:       |          |
   | 0000:0/116       |          |
   |                  |          |
   +------------------+----------+
   | ffff:0000:       | 3        |
   | 0000:0000:       |          |
   | bbbb:bbbb:       |          |
   | 0000:0/116       |          |
   |                  |          |
   +------------------+----------+
   | ffff:0000:       | 2        |
   | 0000:0000:       |          |
   | 5555:5555:       |          |
   | 0000:0/116       |          |
   |                  |          |
   +------------------+----------+
   | ffff:0000:       | 3        |
   | 0000:0000:       |          |
   | 6666:6666:       |          |
   | 0000:0/116       |          |
   |                  |          |
   +------------------+----------+
   | ffff:0000:       | 2        |
   | 1111:1111:       |          |
   | 0000:0000:       |          |
   | 0000:0000/116    |          |
   |                  |          |
   +------------------+----------+
   | ffff:0000:       | 3        |
   | 1111:1111:       |          |
   | 1111:1111:       |          |
   | 0000:0000/116    |          |
   |                  |          |
   +------------------+----------+

.. _table_ipsec_endpoint_inbound_routing:

.. table:: Endpoint 1 Routing Table

   +------------------+----------+
   | **Dst addr**     | **Port** |
   |                  |          |
   +------------------+----------+
   | 172.16.1.5/32    | 0        |
   |                  |          |
   +------------------+----------+
   | 172.16.1.6/32    | 1        |
   |                  |          |
   +------------------+----------+
   | 192.168.185.0/24 | 0        |
   |                  |          |
   +------------------+----------+
   | 192.168.186.0/24 | 1        |
   |                  |          |
   +------------------+----------+
   | 192.168.245.0/24 | 0        |
   |                  |          |
   +------------------+----------+
   | 192.168.246.0/24 | 1        |
   |                  |          |
   +------------------+----------+
   | 192.168.105.0/24 | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.106.0/24 | 3        |
   |                  |          |
   +------------------+----------+
   | 192.168.55.0/24  | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.56.0/24  | 3        |
   |                  |          |
   +------------------+----------+
   | 192.168.175.0/24 | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.176.0/24 | 3        |
   |                  |          |
   +------------------+----------+
   | 192.168.200.0/24 | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.201.0/24 | 3        |
   |                  |          |
   +------------------+----------+
   | 192.168.240.0/24 | 2        |
   |                  |          |
   +------------------+----------+
   | 192.168.241.0/24 | 3        |
   |                  |          |
   +------------------+----------+
   | 1111:1111:       | 0        |
   | 1111:1111:       |          |
   | 1111:1111:       |          |
   | 1111:5555/116    |          |
   |                  |          |
   +------------------+----------+
   | 1111:1111:       | 1        |
   | 1111:1111:       |          |
   | 1111:1111:       |          |
   | 1111:6666/116    |          |
   |                  |          |
   +------------------+----------+
   | ffff:0000:       | 0        |
   | 1111:1111:       |          |
   | 0000:0000:       |          |
   | 0000:0000/116    |          |
   |                  |          |
   +------------------+----------+
   | ffff:0000:       | 1        |
   | 1111:1111:       |          |
   | 1111:1111:       |          |
   | 0000:0000/116    |          |
   |                  |          |
   +------------------+----------+
   | 0000:0000:       | 2        |
   | 0000:0000:       |          |
   | aaaa:aaaa:       |          |
   | 0000:0/116       |          |
   |                  |          |
   +------------------+----------+
   | 0000:0000:       | 3        |
   | 0000:0000:       |          |
   | bbbb:bbbb:       |          |
   | 0000:0/116       |          |
   |                  |          |
   +------------------+----------+
   | 0000:0000:       | 2        |
   | 0000:0000:       |          |
   | 5555:5555:       |          |
   | 0000:0/116       |          |
   |                  |          |
   +------------------+----------+
   | 0000:0000:       | 3        |
   | 0000:0000:       |          |
   | 6666:6666:       |          |
   | 0000:0/116       |          |
   |                  |          |
   +------------------+----------+
   | 0000:0000:       | 2        |
   | 1111:1111:       |          |
   | 0000:0000:       |          |
   | 0000:0000/116    |          |
   |                  |          |
   +------------------+----------+
   | 0000:0000:       | 3        |
   | 1111:1111:       |          |
   | 1111:1111:       |          |
   | 0000:0000/116    |          |
   |                  |          |
   +------------------+----------+
