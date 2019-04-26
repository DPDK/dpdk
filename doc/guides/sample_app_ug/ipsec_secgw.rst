..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2017 Intel Corporation.

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

The application also supports complete IPsec protocol offload to hardware
(Look aside crypto accelerator or using ethernet device). It also support
inline ipsec processing by the supported ethernet device during transmission.
These modes can be selected during the SA creation configuration.

In case of complete protocol offload, the processing of headers(ESP and outer
IP header) is done by the hardware and the application does not need to
add/remove them during outbound/inbound processing.

For inline offloaded outbound traffic, the application will not do the LPM
lookup for routing, as the port on which the packet has to be forwarded will be
part of the SA. Security parameters will be configured on that port only, and
sending the packet on other ports could result in unencrypted packets being
sent out.

The Path for IPsec Inbound traffic is:

*  Read packets from the port.
*  Classify packets between IPv4 and ESP.
*  Perform Inbound SA lookup for ESP packets based on their SPI.
*  Perform Verification/Decryption (Not needed in case of inline ipsec).
*  Remove ESP and outer IP header (Not needed in case of protocol offload).
*  Inbound SP check using ACL of decrypted packets and any other IPv4 packets.
*  Routing.
*  Write packet to port.

The Path for the IPsec Outbound traffic is:

*  Read packets from the port.
*  Perform Outbound SP check using ACL of all IPv4 traffic.
*  Perform Outbound SA lookup for packets that need IPsec protection.
*  Add ESP and outer IP header (Not needed in case protocol offload).
*  Perform Encryption/Digest (Not needed in case of inline ipsec).
*  Routing.
*  Write packet to port.


Constraints
-----------

*  No IPv6 options headers.
*  No AH mode.
*  Supported algorithms: AES-CBC, AES-CTR, AES-GCM, 3DES-CBC, HMAC-SHA1 and NULL.
*  Each SA must be handle by a unique lcore (*1 RX queue per port*).
*  No chained mbufs.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``ipsec-secgw`` sub-directory.

#. [Optional] Build the application for debugging:
   This option adds some extra flags, disables compiler optimizations and
   is verbose::

       make DEBUG=1


Running the Application
-----------------------

The application has a number of command line options::


   ./build/ipsec-secgw [EAL options] --
                        -p PORTMASK -P -u PORTMASK -j FRAMESIZE
                        -l -w REPLAY_WINOW_SIZE -e -a
                        --config (port,queue,lcore)[,(port,queue,lcore]
                        --single-sa SAIDX
                        --rxoffload MASK
                        --txoffload MASK
                        -f CONFIG_FILE_PATH

Where:

*   ``-p PORTMASK``: Hexadecimal bitmask of ports to configure.

*   ``-P``: *optional*. Sets all ports to promiscuous mode so that packets are
    accepted regardless of the packet's Ethernet MAC destination address.
    Without this option, only packets with the Ethernet MAC destination address
    set to the Ethernet address of the port are accepted (default is enabled).

*   ``-u PORTMASK``: hexadecimal bitmask of unprotected ports

*   ``-j FRAMESIZE``: *optional*. Enables jumbo frames with the maximum size
    specified as FRAMESIZE. If an invalid value is provided as FRAMESIZE
    then the default value 9000 is used.

*   ``-l``: enables code-path that uses librte_ipsec.

*   ``-w REPLAY_WINOW_SIZE``: specifies the IPsec sequence number replay window
    size for each Security Association (available only with librte_ipsec
    code path).

*   ``-e``: enables Security Association extended sequence number processing
    (available only with librte_ipsec code path).

*   ``-a``: enables Security Association sequence number atomic behavior
    (available only with librte_ipsec code path).

*   ``--config (port,queue,lcore)[,(port,queue,lcore)]``: determines which queues
    from which ports are mapped to which cores.

*   ``--single-sa SAIDX``: use a single SA for outbound traffic, bypassing the SP
    on both Inbound and Outbound. This option is meant for debugging/performance
    purposes.

*   ``--rxoffload MASK``: RX HW offload capabilities to enable/use on this port
    (bitmask of DEV_RX_OFFLOAD_* values). It is an optional parameter and
    allows user to disable some of the RX HW offload capabilities.
    By default all HW RX offloads are enabled.

*   ``--txoffload MASK``: TX HW offload capabilities to enable/use on this port
    (bitmask of DEV_TX_OFFLOAD_* values). It is an optional parameter and
    allows user to disable some of the TX HW offload capabilities.
    By default all HW TX offloads are enabled.

*   ``-f CONFIG_FILE_PATH``: the full path of text-based file containing all
    configuration items for running the application (See Configuration file
    syntax section below). ``-f CONFIG_FILE_PATH`` **must** be specified.
    **ONLY** the UNIX format configuration file is accepted.


The mapping of lcores to port/queues is similar to other l3fwd applications.

For example, given the following command line::

    ./build/ipsec-secgw -l 20,21 -n 4 --socket-mem 0,2048       \
           --vdev "crypto_null" -- -p 0xf -P -u 0x3      \
           --config="(0,0,20),(1,0,20),(2,0,21),(3,0,21)"       \
           -f /path/to/config_file                              \

where each options means:

*   The ``-l`` option enables cores 20 and 21.

*   The ``-n`` option sets memory 4 channels.

*   The ``--socket-mem`` to use 2GB on socket 1.

*   The ``--vdev "crypto_null"`` option creates virtual NULL cryptodev PMD.

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

*   The ``-f /path/to/config_file`` option enables the application read and
    parse the configuration file specified, and configures the application
    with a given set of SP, SA and Routing entries accordingly. The syntax of
    the configuration file will be explained below in more detail. Please
    **note** the parser only accepts UNIX format text file. Other formats
    such as DOS/MAC format will cause a parse error.

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
            --vdev "crypto_aesni_mb" --vdev "crypto_null" \
	    -- \
            -p 0xf -P -u 0x3 --config="(0,0,20),(1,0,20),(2,0,21),(3,0,21)" \
            -f sample.cfg


Configurations
--------------

The following sections provide the syntax of configurations to initialize
your SP, SA, Routing and Neighbour tables.
Configurations shall be specified in the configuration file to be passed to
the application. The file is then parsed by the application. The successful
parsing will result in the appropriate rules being applied to the tables
accordingly.


Configuration File Syntax
~~~~~~~~~~~~~~~~~~~~~~~~~

As mention in the overview, the Security Policies are ACL rules.
The application parsers the rules specified in the configuration file and
passes them to the ACL table, and replicates them per socket in use.

Following are the configuration file syntax.

General rule syntax
^^^^^^^^^^^^^^^^^^^

The parse treats one line in the configuration file as one configuration
item (unless the line concatenation symbol exists). Every configuration
item shall follow the syntax of either SP, SA, Routing or Neighbour
rules specified below.

The configuration parser supports the following special symbols:

 * Comment symbol **#**. Any character from this symbol to the end of
   line is treated as comment and will not be parsed.

 * Line concatenation symbol **\\**. This symbol shall be placed in the end
   of the line to be concatenated to the line below. Multiple lines'
   concatenation is supported.


SP rule syntax
^^^^^^^^^^^^^^

The SP rule syntax is shown as follows:

.. code-block:: console

    sp <ip_ver> <dir> esp <action> <priority> <src_ip> <dst_ip>
    <proto> <sport> <dport>


where each options means:

``<ip_ver>``

 * IP protocol version

 * Optional: No

 * Available options:

   * *ipv4*: IP protocol version 4
   * *ipv6*: IP protocol version 6

``<dir>``

 * The traffic direction

 * Optional: No

 * Available options:

   * *in*: inbound traffic
   * *out*: outbound traffic

``<action>``

 * IPsec action

 * Optional: No

 * Available options:

   * *protect <SA_idx>*: the specified traffic is protected by SA rule
     with id SA_idx
   * *bypass*: the specified traffic traffic is bypassed
   * *discard*: the specified traffic is discarded

``<priority>``

 * Rule priority

 * Optional: Yes, default priority 0 will be used

 * Syntax: *pri <id>*

``<src_ip>``

 * The source IP address and mask

 * Optional: Yes, default address 0.0.0.0 and mask of 0 will be used

 * Syntax:

   * *src X.X.X.X/Y* for IPv4
   * *src XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX/Y* for IPv6

``<dst_ip>``

 * The destination IP address and mask

 * Optional: Yes, default address 0.0.0.0 and mask of 0 will be used

 * Syntax:

   * *dst X.X.X.X/Y* for IPv4
   * *dst XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX/Y* for IPv6

``<proto>``

 * The protocol start and end range

 * Optional: yes, default range of 0 to 0 will be used

 * Syntax: *proto X:Y*

``<sport>``

 * The source port start and end range

 * Optional: yes, default range of 0 to 0 will be used

 * Syntax: *sport X:Y*

``<dport>``

 * The destination port start and end range

 * Optional: yes, default range of 0 to 0 will be used

 * Syntax: *dport X:Y*

Example SP rules:

.. code-block:: console

    sp ipv4 out esp protect 105 pri 1 dst 192.168.115.0/24 sport 0:65535 \
    dport 0:65535

    sp ipv6 in esp bypass pri 1 dst 0000:0000:0000:0000:5555:5555:\
    0000:0000/96 sport 0:65535 dport 0:65535


SA rule syntax
^^^^^^^^^^^^^^

The successfully parsed SA rules will be stored in an array table.

The SA rule syntax is shown as follows:

.. code-block:: console

    sa <dir> <spi> <cipher_algo> <cipher_key> <auth_algo> <auth_key>
    <mode> <src_ip> <dst_ip> <action_type> <port_id>

where each options means:

``<dir>``

 * The traffic direction

 * Optional: No

 * Available options:

   * *in*: inbound traffic
   * *out*: outbound traffic

``<spi>``

 * The SPI number

 * Optional: No

 * Syntax: unsigned integer number

``<cipher_algo>``

 * Cipher algorithm

 * Optional: Yes, unless <aead_algo> is not used

 * Available options:

   * *null*: NULL algorithm
   * *aes-128-cbc*: AES-CBC 128-bit algorithm
   * *aes-256-cbc*: AES-CBC 256-bit algorithm
   * *aes-128-ctr*: AES-CTR 128-bit algorithm
   * *3des-cbc*: 3DES-CBC 192-bit algorithm

 * Syntax: *cipher_algo <your algorithm>*

``<cipher_key>``

 * Cipher key, NOT available when 'null' algorithm is used

 * Optional: Yes, unless <aead_algo> is not used.
   Must be followed by <cipher_algo> option

 * Syntax: Hexadecimal bytes (0x0-0xFF) concatenate by colon symbol ':'.
   The number of bytes should be as same as the specified cipher algorithm
   key size.

   For example: *cipher_key A1:B2:C3:D4:A1:B2:C3:D4:A1:B2:C3:D4:
   A1:B2:C3:D4*

``<auth_algo>``

 * Authentication algorithm

 * Optional: Yes, unless <aead_algo> is not used

 * Available options:

    * *null*: NULL algorithm
    * *sha1-hmac*: HMAC SHA1 algorithm

``<auth_key>``

 * Authentication key, NOT available when 'null' or 'aes-128-gcm' algorithm
   is used.

 * Optional: Yes, unless <aead_algo> is not used.
   Must be followed by <auth_algo> option

 * Syntax: Hexadecimal bytes (0x0-0xFF) concatenate by colon symbol ':'.
   The number of bytes should be as same as the specified authentication
   algorithm key size.

   For example: *auth_key A1:B2:C3:D4:A1:B2:C3:D4:A1:B2:C3:D4:A1:B2:C3:D4:
   A1:B2:C3:D4*

``<aead_algo>``

 * AEAD algorithm

 * Optional: Yes, unless <cipher_algo> and <auth_algo> are not used

 * Available options:

   * *aes-128-gcm*: AES-GCM 128-bit algorithm

 * Syntax: *cipher_algo <your algorithm>*

``<aead_key>``

 * Cipher key, NOT available when 'null' algorithm is used

 * Optional: Yes, unless <cipher_algo> and <auth_algo> are not used.
   Must be followed by <aead_algo> option

 * Syntax: Hexadecimal bytes (0x0-0xFF) concatenate by colon symbol ':'.
   The number of bytes should be as same as the specified AEAD algorithm
   key size.

   For example: *aead_key A1:B2:C3:D4:A1:B2:C3:D4:A1:B2:C3:D4:
   A1:B2:C3:D4*

``<mode>``

 * The operation mode

 * Optional: No

 * Available options:

   * *ipv4-tunnel*: Tunnel mode for IPv4 packets
   * *ipv6-tunnel*: Tunnel mode for IPv6 packets
   * *transport*: transport mode

 * Syntax: mode XXX

``<src_ip>``

 * The source IP address. This option is not available when
   transport mode is used

 * Optional: Yes, default address 0.0.0.0 will be used

 * Syntax:

   * *src X.X.X.X* for IPv4
   * *src XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX* for IPv6

``<dst_ip>``

 * The destination IP address. This option is not available when
   transport mode is used

 * Optional: Yes, default address 0.0.0.0 will be used

 * Syntax:

   * *dst X.X.X.X* for IPv4
   * *dst XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX* for IPv6

``<type>``

 * Action type to specify the security action. This option specify
   the SA to be performed with look aside protocol offload to HW
   accelerator or protocol offload on ethernet device or inline
   crypto processing on the ethernet device during transmission.

 * Optional: Yes, default type *no-offload*

 * Available options:

   * *lookaside-protocol-offload*: look aside protocol offload to HW accelerator
   * *inline-protocol-offload*: inline protocol offload on ethernet device
   * *inline-crypto-offload*: inline crypto processing on ethernet device
   * *no-offload*: no offloading to hardware

 ``<port_id>``

 * Port/device ID of the ethernet/crypto accelerator for which the SA is
   configured. For *inline-crypto-offload* and *inline-protocol-offload*, this
   port will be used for routing. The routing table will not be referred in
   this case.

 * Optional: No, if *type* is not *no-offload*

 * Syntax:

   * *port_id X* X is a valid device number in decimal


Example SA rules:

.. code-block:: console

    sa out 5 cipher_algo null auth_algo null mode ipv4-tunnel \
    src 172.16.1.5 dst 172.16.2.5

    sa out 25 cipher_algo aes-128-cbc \
    cipher_key c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3 \
    auth_algo sha1-hmac \
    auth_key c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3:c3 \
    mode ipv6-tunnel \
    src 1111:1111:1111:1111:1111:1111:1111:5555 \
    dst 2222:2222:2222:2222:2222:2222:2222:5555

    sa in 105 aead_algo aes-128-gcm \
    aead_key de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
    mode ipv4-tunnel src 172.16.2.5 dst 172.16.1.5

    sa out 5 cipher_algo aes-128-cbc cipher_key 0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0 \
    auth_algo sha1-hmac auth_key 0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0 \
    mode ipv4-tunnel src 172.16.1.5 dst 172.16.2.5 \
    type lookaside-protocol-offload port_id 4

    sa in 35 aead_algo aes-128-gcm \
    aead_key de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
    mode ipv4-tunnel src 172.16.2.5 dst 172.16.1.5 \
    type inline-crypto-offload port_id 0

Routing rule syntax
^^^^^^^^^^^^^^^^^^^

The Routing rule syntax is shown as follows:

.. code-block:: console

    rt <ip_ver> <src_ip> <dst_ip> <port>


where each options means:

``<ip_ver>``

 * IP protocol version

 * Optional: No

 * Available options:

   * *ipv4*: IP protocol version 4
   * *ipv6*: IP protocol version 6

``<src_ip>``

 * The source IP address and mask

 * Optional: Yes, default address 0.0.0.0 and mask of 0 will be used

 * Syntax:

   * *src X.X.X.X/Y* for IPv4
   * *src XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX/Y* for IPv6

``<dst_ip>``

 * The destination IP address and mask

 * Optional: Yes, default address 0.0.0.0 and mask of 0 will be used

 * Syntax:

   * *dst X.X.X.X/Y* for IPv4
   * *dst XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX/Y* for IPv6

``<port>``

 * The traffic output port id

 * Optional: yes, default output port 0 will be used

 * Syntax: *port X*

Example SP rules:

.. code-block:: console

    rt ipv4 dst 172.16.1.5/32 port 0

    rt ipv6 dst 1111:1111:1111:1111:1111:1111:1111:5555/116 port 0

Neighbour rule syntax
^^^^^^^^^^^^^^^^^^^^^

The Neighbour rule syntax is shown as follows:

.. code-block:: console

    neigh <port> <dst_mac>


where each options means:

``<port>``

 * The output port id

 * Optional: No

 * Syntax: *port X*

``<dst_mac>``

 * The destination ethernet address to use for that port

 * Optional: No

 * Syntax:

   * XX:XX:XX:XX:XX:XX

Example Neighbour rules:

.. code-block:: console

    neigh port 0 DE:AD:BE:EF:01:02

Test directory
--------------

The test directory contains scripts for testing the various encryption
algorithms.

The purpose of the scripts is to automate ipsec-secgw testing
using another system running linux as a DUT.

The user must setup the following environment variables:

*   ``SGW_PATH``: path to the ipsec-secgw binary to test.

*   ``REMOTE_HOST``: IP address/hostname of the DUT.

*   ``REMOTE_IFACE``: interface name for the test-port on the DUT.

*   ``ETH_DEV``: ethernet device to be used on the SUT by DPDK ('-w <pci-id>')

Also the user can optionally setup:

*   ``SGW_LCORE``: lcore to run ipsec-secgw on (default value is 0)

*   ``CRYPTO_DEV``: crypto device to be used ('-w <pci-id>'). If none specified
    appropriate vdevs will be created by the script

Note that most of the tests require the appropriate crypto PMD/device to be
available.

Server configuration
~~~~~~~~~~~~~~~~~~~~

Two servers are required for the tests, SUT and DUT.

Make sure the user from the SUT can ssh to the DUT without entering the password.
To enable this feature keys must be setup on the DUT.

``ssh-keygen`` will make a private & public key pair on the SUT.

``ssh-copy-id`` <user name>@<target host name> on the SUT will copy the public
key to the DUT. It will ask for credentials so that it can upload the public key.

The SUT and DUT are connected through at least 2 NIC ports.

One NIC port is expected to be managed by linux on both machines and will be
used as a control path.

The second NIC port (test-port) should be bound to DPDK on the SUT, and should
be managed by linux on the DUT.

The script starts ``ipsec-secgw`` with 2 NIC devices: ``test-port`` and
``tap vdev``.

It then configures the local tap interface and the remote interface and IPsec
policies in the following way:

Traffic going over the test-port in both directions has to be protected by IPsec.

Traffic going over the TAP port in both directions does not have to be protected.

i.e:

DUT OS(NIC1)--(IPsec)-->(NIC1)ipsec-secgw(TAP)--(plain)-->(TAP)SUT OS

SUT OS(TAP)--(plain)-->(TAP)psec-secgw(NIC1)--(IPsec)-->(NIC1)DUT OS

It then tries to perform some data transfer using the scheme described above.

usage
~~~~~

In the ipsec-secgw/test directory

to run one test for IPv4 or IPv6

/bin/bash linux_test(4|6).sh <ipsec_mode>

to run all tests for IPv4 or IPv6

/bin/bash run_test.sh -4|-6

For the list of available modes please refer to run_test.sh.
