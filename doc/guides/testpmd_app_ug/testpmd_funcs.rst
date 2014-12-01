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

Testpmd Runtime Functions
=========================

Where the testpmd application is started in interactive mode, (-i|--interactive),
it displays a prompt that can be used to start and stop forwarding,
configure the application, display statistics, set the Flow Director and other tasks.

.. code-block:: console

    testpmd>

The testpmd prompt has some, limited, readline support.
Common bash command- line functions such as Ctrl+a and Ctrl+e to go to the start and end of the prompt line are supported
as well as access to the command history via the up-arrow.

There is also support for tab completion.
If you type a partial command and hit <TAB> you get a list of the available completions:

.. code-block:: console

    testpmd> show port <TAB>

        info [Mul-choice STRING]: show|clear port info|stats|fdir|stat_qmap X
        info [Mul-choice STRING]: show|clear port info|stats|fdir|stat_qmap all
        stats [Mul-choice STRING]: show|clear port info|stats|fdir|stat_qmap X
        stats [Mul-choice STRING]: show|clear port info|stats|fdir|stat_qmap all
        ...

Help Functions
--------------

The testpmd has on-line help for the functions that are available at runtime.
These are divided into sections and can be accessed using help, help section or help all:

.. code-block:: console

    testpmd> help

        Help is available for the following sections:
        help control    : Start and stop forwarding.
        help display    : Displaying port, stats and config information.
        help config     : Configuration information.
        help ports      : Configuring ports.
        help flowdir    : Flow Director filter help.
        help registers  : Reading and setting port registers.
        help filters    : Filters configuration help.
        help all        : All of the above sections.

Control Functions
-----------------

start
~~~~~

Start packet forwarding with current configuration:

start

start tx_first
~~~~~~~~~~~~~~

Start packet forwarding with current configuration after sending one burst of packets:

start tx_first

stop
~~~~

Stop packet forwarding, and display accumulated statistics:

stop

quit
~~~~

Quit to prompt:

quit

Display Functions
-----------------

The functions in the following sections are used to display information about the
testpmd configuration or the NIC status.

show port
~~~~~~~~~

Display information for a given port or all ports:

show port (info|stats|fdir|stat_qmap) (port_id|all)

The available information categories are:

info    : General port information such as MAC address.

stats   : RX/TX statistics.

fdir    : Flow Director information and statistics.

stat_qmap : Queue statistics mapping.

For example:

.. code-block:: console

    testpmd> show port info 0

    ********************* Infos for port 0 *********************

    MAC address: XX:XX:XX:XX:XX:XX
    Link status: up
    Link speed: 10000 Mbps
    Link duplex: full-duplex
    Promiscuous mode: enabled
    Allmulticast mode: disabled
    Maximum number of MAC addresses: 127
    VLAN offload:
        strip on
        filter on
        qinq(extend) off

show port rss reta
~~~~~~~~~~~~~~~~~~

Display the rss redirection table entry indicated by masks on port X:

show port (port_id) rss reta (size) (mask0, mask1...)

size is used to indicate the hardware supported reta size

show port rss-hash
~~~~~~~~~~~~~~~~~~

Display the RSS hash functions and RSS hash key of a port:

show port (port_id) rss-hash [key]

clear port
~~~~~~~~~~

Clear the port statistics for a given port or for all ports:

clear port (info|stats|fdir|stat_qmap) (port_id|all)

For example:

.. code-block:: console

    testpmd> clear port stats all

show config
~~~~~~~~~~~

Displays the configuration of the application.
The configuration comes from the command-line, the runtime or the application defaults:

show config (rxtx|cores|fwd)

The available information categories are:

rxtx  : RX/TX configuration items.

cores : List of forwarding cores.

fwd   : Packet forwarding configuration.

For example:

.. code-block:: console

    testpmd> show config rxtx

    io packet forwarding - CRC stripping disabled - packets/burst=16
    nb forwarding cores=2 - nb forwarding ports=1
    RX queues=1 - RX desc=128 - RX free threshold=0
    RX threshold registers: pthresh=8 hthresh=8 wthresh=4
    TX queues=1 - TX desc=512 - TX free threshold=0
    TX threshold registers: pthresh=36 hthresh=0 wthresh=0
    TX RS bit threshold=0 - TXQ flags=0x0

read rxd
~~~~~~~~

Display an RX descriptor for a port RX queue:

read rxd (port_id) (queue_id) (rxd_id)

For example:

.. code-block:: console

    testpmd> read rxd 0 0 4
        0x0000000B - 0x001D0180 / 0x0000000B - 0x001D0180

read txd
~~~~~~~~

Display a TX descriptor for a port TX queue:

read txd (port_id) (queue_id) (txd_id)

For example:

.. code-block:: console

    testpmd> read txd 0 0 4
        0x00000001 - 0x24C3C440 / 0x000F0000 - 0x2330003C

Configuration Functions
-----------------------

The testpmd application can be configured from the runtime as well as from the command-line.

This section details the available configuration functions that are available.

.. note::

    Configuration changes only become active when forwarding is started/restarted.

set default
~~~~~~~~~~~

Reset forwarding to the default configuration:

set default

set verbose
~~~~~~~~~~~

Set the debug verbosity level:

set verbose (level)

Currently the only available levels are 0 (silent except for error) and 1 (fully verbose).

set nbport
~~~~~~~~~~

Set the number of ports used by the application:

set nbport (num)

This is equivalent to the --nb-ports command-line option.

set nbcore
~~~~~~~~~~

Set the number of cores used by the application:

set nbcore (num)

This is equivalent to the --nb-cores command-line option.

.. note::

    The number of cores used must not be greater than number of ports used multiplied by the number of queues per port.

set coremask
~~~~~~~~~~~~

Set the forwarding cores hexadecimal mask:

set coremask (mask)

This is equivalent to the --coremask command-line option.

.. note::

    The master lcore is reserved for command line parsing only and cannot be masked on for packet forwarding.

set portmask
~~~~~~~~~~~~

Set the forwarding ports hexadecimal mask:

set portmask (mask)

This is equivalent to the --portmask command-line option.

set burst
~~~~~~~~~

Set number of packets per burst:

set burst (num)

This is equivalent to the --burst command-line option.

In mac_retry forwarding mode, the transmit delay time and number of retries can also be set.

set burst tx delay (micrseconds) retry (num)

set txpkts
~~~~~~~~~~

Set the length of each segment of the TX-ONLY packets:

set txpkts (x[,y]*)

Where x[,y]* represents a CSV list of values, without white space.

set corelist
~~~~~~~~~~~~

Set the list of forwarding cores:

set corelist (x[,y]*)

For example, to change the forwarding cores:

.. code-block:: console

    testpmd> set corelist 3,1
    testpmd> show config fwd

    io packet forwarding - ports=2 - cores=2 - streams=2 - NUMA support disabled
    Logical Core 3 (socket 0) forwards packets on 1 streams:
    RX P=0/Q=0 (socket 0) -> TX P=1/Q=0 (socket 0) peer=02:00:00:00:00:01
    Logical Core 1 (socket 0) forwards packets on 1 streams:
    RX P=1/Q=0 (socket 0) -> TX P=0/Q=0 (socket 0) peer=02:00:00:00:00:00

.. note::

    The cores are used in the same order as specified on the command line.

set portlist
~~~~~~~~~~~~

Set the list of forwarding ports:

set portlist (x[,y]*)

For example, to change the port forwarding:

.. code-block:: console

    testpmd> set portlist 0,2,1,3
    testpmd> show config fwd

    io packet forwarding - ports=4 - cores=1 - streams=4
    Logical Core 3 (socket 0) forwards packets on 4 streams:
    RX P=0/Q=0 (socket 0) -> TX P=2/Q=0 (socket 0) peer=02:00:00:00:00:01
    RX P=2/Q=0 (socket 0) -> TX P=0/Q=0 (socket 0) peer=02:00:00:00:00:00
    RX P=1/Q=0 (socket 0) -> TX P=3/Q=0 (socket 0) peer=02:00:00:00:00:03
    RX P=3/Q=0 (socket 0) -> TX P=1/Q=0 (socket 0) peer=02:00:00:00:00:02

vlan set strip
~~~~~~~~~~~~~~

Set the VLAN strip on a port:

vlan set strip (on|off) (port_id)

vlan set stripq
~~~~~~~~~~~~~~~

Set the VLAN strip for a queue on a port:

vlan set stripq (on|off) (port_id,queue_id)

vlan set filter
~~~~~~~~~~~~~~~

Set the VLAN filter on a port:

vlan set filter (on|off) (port_id)

vlan set qinq
~~~~~~~~~~~~~

Set the VLAN QinQ (extended queue in queue) on for a port:

vlan set qinq (on|off) (port_id)

vlan set tpid
~~~~~~~~~~~~~

Set the outer VLAN TPID for packet filtering on a port:

vlan set tpid (value) (port_id)

.. note::

    TPID value must be a 16-bit number (value <= 65536).

rx_vlan add
~~~~~~~~~~~

Add a VLAN ID, or all identifiers, to the set of VLAN identifiers filtered by port ID:

rx_vlan add (vlan_id|all) (port_id)

.. note::

    VLAN filter must be set on that port. VLAN ID < 4096.

rx_vlan rm
~~~~~~~~~~

Remove a VLAN ID, or all identifiers, from the set of VLAN identifiers filtered by port ID:

rx_vlan rm (vlan_id|all) (port_id)

rx_vlan add(for VF)
~~~~~~~~~~~~~~~~~~~

Add a VLAN ID, to the set of VLAN identifiers filtered for VF(s) for port ID:

rx_vlan add (vlan_id) port (port_id) vf (vf_mask)

rx_vlan rm(for VF)
~~~~~~~~~~~~~~~~~~

Remove a VLAN ID, from the set of VLAN identifiers filtered for VF(s) for port ID:

rx_vlan rm (vlan_id) port (port_id) vf (vf_mask)

rx_vlan set tpid
~~~~~~~~~~~~~~~~

Set the outer VLAN TPID for packet filtering on a port:

rx_vlan set tpid (value) (port_id)

tunnel_filter add
~~~~~~~~~~~~~~~~~

Add a tunnel filter on a port:

tunnel_filter add (port_id) (outer_mac) (inner_mac) (ip_addr) (inner_vlan)
 (tunnel_type) (filter_type) (tenant_id) (queue_id)

tunnel_filter remove
~~~~~~~~~~~~~~~~~~~~

Remove a tunnel filter on a port:

tunnel_filter rm (port_id) (outer_mac) (inner_mac) (ip_addr) (inner_vlan)
 (tunnel_type) (filter_type) (tenant_id) (queue_id)

rx_vxlan_port add
~~~~~~~~~~~~~~~~~

Add an UDP port for VXLAN packet filter on a port:

rx_vxlan_port add (udp_port) (port_id)

rx_vxlan_port remove
~~~~~~~~~~~~~~~~~~~~

Remove an UDP port for VXLAN packet filter on a port:

rx_vxlan_port rm (udp_port) (port_id)

tx_vlan set
~~~~~~~~~~~

Set hardware insertion of VLAN ID in packets sent on a port:

tx_vlan set (vlan_id) (port_id)

tx_vlan set pvid
~~~~~~~~~~~~~~~~

Set port based hardware insertion of VLAN ID in pacekts sent on a port:

tx_vlan set pvid (port_id) (vlan_id) (on|off)

tx_vlan reset
~~~~~~~~~~~~~

Disable hardware insertion of a VLAN header in packets sent on a port:

tx_vlan reset (port_id)

tx_checksum set
~~~~~~~~~~~~~~~

Select hardware or software calculation of the checksum when
transmitting a packet using the csum forward engine:

tx_cksum set (ip|udp|tcp|sctp|vxlan)

ip|udp|tcp|sctp always concern the inner layer.
vxlan concerns the outer IP and UDP layer (in case the packet
is recognized as a vxlan packet by the forward engine)

.. note::

    Check the NIC Datasheet for hardware limits.

tx_checksum show
~~~~~~~~~~~~~~~~

Display tx checksum offload configuration:

tx_checksum show (port_id)

tso set
~~~~~~~

Enable TCP Segmentation Offload in csum forward engine:

tso set (segsize) (port_id)

.. note::
   Please check the NIC datasheet for HW limits

tso show
~~~~~~~~

Display the status of TCP Segmentation Offload:

tso show (port_id)

set fwd
~~~~~~~

Set the packet forwarding mode:

set fwd (io|mac|mac_retry|macswap|flowgen|rxonly|txonly|csum|icmpecho)

The available information categories are:

*   io: forwards packets "as-is" in I/O mode.
    This is the fastest possible forwarding operation as it does not access packets data.
    This is the default mode.

*   mac: changes the source and the destination Ethernet addresses of packets before forwarding them.

*   mac_retry: same as "mac" forwarding mode, but includes retries if the destination queue is full.

*   macswap: MAC swap forwarding mode.
    Swaps the source and the destination Ethernet addresses of packets before forwarding them.

*   flowgen: multi-flow generation mode.
    Originates a bunch of flows (varying destination IP addresses), and terminate receive traffic.

*   rxonly: receives packets but doesn't transmit them.

*   txonly: generates and transmits packets without receiving any.

*   csum: changes the checksum field with HW or SW methods depending on the offload flags on the packet.

*   icmpecho: receives a burst of packets, lookup for IMCP echo requests and, if any, send back ICMP echo replies.


Example:

.. code-block:: console

    testpmd> set fwd rxonly

    Set rxonly packet forwarding mode

mac_addr add
~~~~~~~~~~~~

Add an alternative MAC address to a port:

mac_addr add (port_id) (XX:XX:XX:XX:XX:XX)

mac_addr remove
~~~~~~~~~~~~~~~

Remove a MAC address from a port:

mac_addr remove (port_id) (XX:XX:XX:XX:XX:XX)

mac_addr add(for VF)
~~~~~~~~~~~~~~~~~~~~

Add an alternative MAC address for a VF to a port:

mac_add add port (port_id) vf (vf_id) (XX:XX:XX:XX:XX:XX)

set port-uta
~~~~~~~~~~~~

Set the unicast hash filter(s) on/off for a port X:

set port (port_id) uta (XX:XX:XX:XX:XX:XX|all) (on|off)

set promisc
~~~~~~~~~~~

Set the promiscuous mode on for a port or for all ports.
In promiscuous mode packets are not dropped if they aren't for the specified MAC address:

set promisc (port_id|all) (on|off)

set allmulti
~~~~~~~~~~~~

Set the allmulti mode for a port or for all ports:

set allmulti (port_id|all) (on|off)

Same as the ifconfig (8) option. Controls how multicast packets are handled.

set flow_ctrl rx
~~~~~~~~~~~~~~~~

Set the link flow control parameter on a port:

set flow_ctrl rx (on|off) tx (on|off) (high_water) (low_water) \
(pause_time) (send_xon) (port_id)

Where:

high_water (integer): High threshold value to trigger XOFF.

low_water (integer) : Low threshold value to trigger XON.

pause_time (integer): Pause quota in the Pause frame.

send_xon (0/1) : Send XON frame.

mac_ctrl_frame_fwd : Enable receiving MAC control frames

set pfc_ctrl rx
~~~~~~~~~~~~~~~

Set the priority flow control parameter on a port:

set pfc_ctrl rx (on|off) tx (on|off) (high_water) (low_water) \ (pause_time) (priority) (port_id)

Where:

priority (0-7): VLAN User Priority.

set stat_qmap
~~~~~~~~~~~~~

Set statistics mapping (qmapping 0..15) for RX/TX queue on port:

set stat_qmap (tx|rx) (port_id) (queue_id) (qmapping)

For example, to set rx queue 2 on port 0 to mapping 5:

.. code-block:: console

     testpmd>set stat_qmap rx 0 2 5

set port - rx/tx(for VF)
~~~~~~~~~~~~~~~~~~~~~~~~

Set VF receive/transmit from a port:

set port (port_id) vf (vf_id) (rx|tx) (on|off)

set port - mac address filter (for VF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add/Remove unicast or multicast MAC addr filter for a VF:

set port (port_id) vf (vf_id) (mac_addr)
 (exact-mac|exact-mac-vlan|hashmac|hashmac-vlan) (on|off)

set port - rx mode(for VF)
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the VF receive mode of a port:

set port (port_id) vf (vf_id) rxmode (AUPE|ROPE|BAM|MPE) (on|off)

The available receive modes are:

*  AUPE: accepts untagged VLAN.

*  ROPE: accepts unicast hash.

*  BAM: accepts broadcast packets

*  MPE: accepts all multicast packets

set port - tx_rate (for Queue)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set TX rate limitation for queue of a port ID:

set port (port_id) queue (queue_id) rate (rate_value)

set port - tx_rate (for VF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set TX rate limitation for queues in VF of a port ID:

set port (port_id) vf (vf_id) rate (rate_value) queue_mask (queue_mask)

set port - mirror rule
~~~~~~~~~~~~~~~~~~~~~~

Set port or vlan type mirror rule for a port.

set port (port_id) mirror-rule (rule_id) (pool-mirror|vlan-mirror) (poolmask|vlanid[,vlanid]*) dst-pool (pool_id) (on|off)

For example to enable mirror traffic with vlan 0,1 to pool 0:

.. code-block:: console

    set port 0 mirror-rule 0 vlan-mirror 0,1 dst-pool 0 on

reset port - mirror rule
~~~~~~~~~~~~~~~~~~~~~~~~

Reset a mirror rule for a port.

reset port (port_id) mirror-rule (rule_id)

set flush_rx
~~~~~~~~~~~~

Flush (default) or don't flush RX streams before forwarding.
Mainly used with PCAP drivers to avoid the default behavior of flushing the first 512 packets on RX streams.

set flush_rx off

set bypass mode
~~~~~~~~~~~~~~~

Set the bypass mode for the lowest port on bypass enabled NIC.

set bypass mode (normal|bypass|isolate) (port_id)

set bypass event
~~~~~~~~~~~~~~~~

Set the event required to initiate specified bypass mode for the lowest port on a bypass enabled NIC where:

*   timeout: enable bypass after watchdog timeout.

*   os_on: enable bypass when OS/board is powered on.

*   os_off: enable bypass when OS/board is powered off.

*   power_on: enable bypass when power supply is turned on.

*   power_off: enable bypass when power supply is turned off.

set bypass event (timeout|os_on|os_off|power_on|power_off) mode (normal|bypass|isolate) (port_id)

set bypass timeout
~~~~~~~~~~~~~~~~~~

Set the bypass watchdog timeout to 'n' seconds where 0 = instant.

set bypass timeout (0|1.5|2|3|4|8|16|32)

show bypass config
~~~~~~~~~~~~~~~~~~

Show the bypass configuration for a bypass enabled NIC using the lowest port on the NIC.

show bypass config (port_id)

set link up
~~~~~~~~~~~

Set link up for a port.

set link-up port (port id)

set link down
~~~~~~~~~~~~~

Set link down for a port.

set link-down port (port id)

Port Functions
--------------

The following sections show functions for configuring ports.

.. note::

    Port configuration changes only become active when forwarding is started/restarted.

port start
~~~~~~~~~~

Start all ports or a specific port:

port start (port_id|all)

port stop
~~~~~~~~~

Stop all ports or a specific port:

port stop (port_id|all)

port close
~~~~~~~~~~

Close all ports or a specific port:

port close (port_id|all)

port start/stop queue
~~~~~~~~~~~~~~~~~~~~~

Start/stop a rx/tx queue on a specific port:

port (port_id) (rxq|txq) (queue_id) (start|stop)

Only take effect when port is started.

port config - speed
~~~~~~~~~~~~~~~~~~~

Set the speed and duplex mode for all ports or a specific port:

port config (port_id|all) speed (10|100|1000|10000|auto) duplex (half|full|auto)

port config - queues/descriptors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set number of queues/descriptors for rxq, txq, rxd and txd:

port config all (rxq|txq|rxd|txd) (value)

This is equivalent to the --rxq, --txq, --rxd and --txd command-line options.

port config - max-pkt-len
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the maximum packet length:

port config all max-pkt-len (value)

This is equivalent to the --max-pkt-len command-line option.

port config - CRC Strip
~~~~~~~~~~~~~~~~~~~~~~~

Set hardware CRC stripping on or off for all ports:

port config all crc-strip (on|off)

CRC stripping is off by default.

The on option is equivalent to the --crc-strip command-line option.

port config - RX Checksum
~~~~~~~~~~~~~~~~~~~~~~~~~

Set hardware RX checksum offload to on or off for all ports:

port config all rx-cksum (on|off)

Checksum offload is off by default.

The on option is equivalent to the --enable-rx-cksum command-line option.

port config - VLAN
~~~~~~~~~~~~~~~~~~

Set hardware VLAN on or off for all ports:

port config all hw-vlan (on|off)

Hardware VLAN is on by default.

The off option is equivalent to the --disable-hw-vlan command-line option.

port config - Drop Packets
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set packet drop for packets with no descriptors on or off for all ports:

port config all drop-en (on|off)

Packet dropping for packets with no descriptors is off by default.

The on option is equivalent to the --enable-drop-en command-line option.

port config - RSS
~~~~~~~~~~~~~~~~~

Set the RSS (Receive Side Scaling) mode on or off:

port config all rss (ip|udp|none)

RSS is on by default.

The off option is equivalent to the --disable-rss command-line option.

port config - RSS Reta
~~~~~~~~~~~~~~~~~~~~~~

Set the RSS (Receive Side Scaling) redirection table:

port config all rss reta (hash,queue)[,(hash,queue)]

port config - DCB
~~~~~~~~~~~~~~~~~

Set the DCB mode for an individual port:

port config (port_id) dcb vt (on|off) (traffic_class) pfc (on|off)

The traffic class should be 4 or 8.

port config - Burst
~~~~~~~~~~~~~~~~~~~

Set the number of packets per burst:

port config all burst (value)

This is equivalent to the --burst command-line option.

port config - Threshold
~~~~~~~~~~~~~~~~~~~~~~~

Set thresholds for TX/RX queues:

port config all (threshold) (value)

Where the threshold type can be:

*   txpt: Set the prefetch threshold register of the TX rings, 0 <= value <= 255.

*   txht: Set the host threshold register of the TX rings, 0 <= value <= 255.

*   txwt: Set the write-back threshold register of the TX rings, 0 <= value <= 255.

*   rxpt: Set the prefetch threshold register of the RX rings, 0 <= value <= 255.

*   rxht: Set the host threshold register of the RX rings, 0 <= value <= 255.

*   rxwt: Set the write-back threshold register of the RX rings, 0 <= value <= 255.

*   txfreet: Set the transmit free threshold of the TX rings, 0 <= value <= txd.

*   rxfreet: Set the transmit free threshold of the RX rings, 0 <= value <= rxd.

*   txrst: Set the transmit RS bit threshold of TX rings, 0 <= value <= txd.
    These threshold options are also available from the command-line.

Flow Director Functions
-----------------------

The Flow Director works in receive mode to identify specific flows or sets of flows and route them to specific queues.

Two types of filtering are supported which are referred to as Perfect Match and Signature filters:

*   Perfect match filters.
    The hardware checks a match between the masked fields of the received packets and the programmed filters.

*   Signature filters.
    The hardware checks a match between a hash-based signature of the masked fields of the received packet.

The Flow Director filters can match the following fields in a packet:

*   Source IP and destination IP addresses.

*   Source port and destination port numbers (for UDP and TCP packets).

*   IPv4/IPv6 and UDP/ TCP/SCTP protocol match.

*   VLAN header.

*   Flexible 2-byte tuple match anywhere in the first 64 bytes of the packet.

The Flow Director can also mask out parts of all of these fields so that filters are only applied to certain fields
or parts of the fields.
For example it is possible to mask out sub-nets of IP addresses or to ignore VLAN headers.

In the following sections, several common parameters are used in the Flow Director filters.
These are explained below:

*   src: A pair of source address values. The source IP, in IPv4 or IPv6 format, and the source port:

    src 192.168.0.1 1024

    src 2001:DB8:85A3:0:0:8A2E:370:7000 1024

*   dst: A pair of destination address values. The destination IP, in IPv4 or IPv6 format, and the destination port.

*   flexbytes: A 2-byte tuple to be matched within the first 64 bytes of a packet.

The offset where the match occurs is set by the --pkt-filter-flexbytes-offset command-line parameter
and is counted from the first byte of the destination Ethernet MAC address.
The default offset is 0xC bytes, which is the "Type" word in the MAC header.
Typically, the flexbyte value is set to 0x0800 to match the IPv4 MAC type or 0x86DD to match IPv6.
These values change when a VLAN tag is added.

*   vlan: The VLAN header to match in the packet.

*   queue: The index of the RX queue to route matched packets to.

*   soft: The 16-bit value in the MBUF flow director ID field for RX packets matching the filter.

add_signature_filter
~~~~~~~~~~~~~~~~~~~~

Add a signature filter:

# Command is displayed on several lines for clarity.

add_signature_filter (port_id) (ip|udp|tcp|sctp)

    src (src_ip_address) (src_port)

    dst (dst_ip_address) (dst_port)

    flexbytes (flexbytes_values)

    vlan (vlan_id) queue (queue_id)

upd_signature_filter
~~~~~~~~~~~~~~~~~~~~

Update a signature filter:

# Command is displayed on several lines for clarity.

upd_signature_filter (port_id) (ip|udp|tcp|sctp)

    src (src_ip_address) (src_port)

    dst (dst_ip_address) (dst_port)

    flexbytes (flexbytes_values)

    vlan (vlan_id) queue (queue_id)

rm_signature_filter
~~~~~~~~~~~~~~~~~~~

Remove a signature filter:

# Command is displayed on several lines for clarity.

rm_signature_filter (port_id) (ip|udp|tcp|sctp)

    src (src_ip_address) (src_port)

    dst (dst_ip_address) (dst_port)

    flexbytes (flexbytes_values)

    vlan (vlan_id)

add_perfect_filter
~~~~~~~~~~~~~~~~~~

Add a perfect filter:

# Command is displayed on several lines for clarity.

add_perfect_filter (port_id) (ip|udp|tcp|sctp)

    src (src_ip_address) (src_port)

    dst (dst_ip_address) (dst_port)

    flexbytes (flexbytes_values)

    vlan (vlan_id) queue (queue_id) soft (soft_id)

upd_perfect_filter
~~~~~~~~~~~~~~~~~~

Update a perfect filter:

# Command is displayed on several lines for clarity.

upd_perfect_filter (port_id) (ip|udp|tcp|sctp)

    src (src_ip_address) (src_port)

    dst (dst_ip_address) (dst_port)

    flexbytes (flexbytes_values)

    vlan (vlan_id) queue (queue_id)

rm_perfect_filter
~~~~~~~~~~~~~~~~~

Remove a perfect filter:

rm_perfect_filter (port_id) (ip|udp|tcp|sctp)

    src (src_ip_address) (src_port)

    dst (dst_ip_address) (dst_port)

    flexbytes (flexbytes_values)

    vlan (vlan_id) soft (soft_id)

set_masks_filter
~~~~~~~~~~~~~~~~

Set IPv4 filter masks:

# Command is displayed on several lines for clarity.

set_masks_filter (port_id) only_ip_flow (0|1)

    src_mask (ip_src_mask) (src_port_mask)

    dst_mask (ip_dst_mask) (dst_port_mask)

    flexbytes (0|1) vlan_id (0|1) vlan_prio (0|1)

set_ipv6_masks_filter
~~~~~~~~~~~~~~~~~~~~~

Set IPv6 filter masks:

# Command is displayed on several lines for clarity.

set_ipv6_masks_filter (port_id) only_ip_flow (0|1)

    src_mask (ip_src_mask) (src_port_mask)

    dst_mask (ip_dst_mask) (dst_port_mask)

    flexbytes (0|1) vlan_id (0|1) vlan_prio (0|1)

    compare_dst (0|1)

Link Bonding Functions
----------------------

The Link Bonding functions make it possible to dynamically create and
manage link bonding devices from within testpmd interactive prompt.

create bonded device
~~~~~~~~~~~~~~~~~~~~

Create a new bonding device:

create bonded device (mode) (socket)

For example, to create a bonded device in mode 1 on socket 0.

.. code-block:: console

    testpmd> create bonded 1 0
    created new bonded device (port X)

add bonding slave
~~~~~~~~~~~~~~~~~

Adds Ethernet device to a Link Bonding device:

add bonding slave (slave id) (port id)

For example, to add Ethernet device (port 6) to a Link Bonding device (port 10).

.. code-block:: console

    testpmd> add bonding slave 6 10


remove bonding slave
~~~~~~~~~~~~~~~~~~~~

Removes an Ethernet slave device from a Link Bonding device:

remove bonding slave (slave id) (port id)

For example, to remove Ethernet slave device (port 6) to a Link Bonding device (port 10).

.. code-block:: console

    testpmd> remove bonding slave 6 10

set bonding mode
~~~~~~~~~~~~~~~~

Set the Link Bonding mode of a Link Bonding device:

set bonding mode (value) (port id)

For example, to set the bonding mode of a Link Bonding device (port 10) to broadcast (mode 3).

.. code-block:: console

    testpmd> set bonding mode 3 10

set bonding primary
~~~~~~~~~~~~~~~~~~~

Set an Ethernet slave device as the primary device on a Link Bonding device:

set bonding primary (slave id) (port id)

For example, to set the Ethernet slave device (port 6) as the primary port of a Link Bonding device (port 10).

.. code-block:: console

    testpmd> set bonding primary 6 10

set bonding mac
~~~~~~~~~~~~~~~

Set the MAC address of a Link Bonding device:

set bonding mac (port id) (mac)

For example, to set the MAC address of a Link Bonding device (port 10) to 00:00:00:00:00:01

.. code-block:: console

    testpmd> set bonding mac 10 00:00:00:00:00:01

set bonding xmit_balance_policy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the transmission policy for a Link Bonding device when it is in Balance XOR mode:

set bonding xmit_balance_policy (port_id) (l2|l23|l34)

For example, set a Link Bonding device (port 10) to use a balance policy of layer 3+4 (IP addresses & UDP ports )

.. code-block:: console

    testpmd> set bonding xmit_balance_policy 10 l34


set bonding mon_period
~~~~~~~~~~~~~~~~~~~~~~

Set the link status monitoring polling period in milliseconds for a bonding devicie.

This adds support for PMD slave devices which do not support link status interrupts.
When the mon_period is set to a value greater than 0 then all PMD's which do not support
link status ISR will be queried every polling interval to check if their link status has changed.

set bonding mon_period (port_id) (value)

For example, to set the link status monitoring polling period of bonded device (port 5) to 150ms

.. code-block:: console

    testpmd> set bonding mon_period 5 150


show bonding config
~~~~~~~~~~~~~~~~~~~

Show the current configuration of a Link Bonding device:

show bonding config (port id)

For example,
to show the configuration a Link Bonding device (port 9) with 3 slave devices (1, 3, 4)
in balance mode with a transmission policy of layer 2+3.

.. code-block:: console

    testpmd> show bonding config 9
        Bonding mode: 2
        Balance Xmit Policy: BALANCE_XMIT_POLICY_LAYER23
        Slaves (3): [1 3 4]
        Active Slaves (3): [1 3 4]
        Primary: [3]

Register Functions
------------------

The Register functions can be used to read from and write to registers on the network card referenced by a port number.
This is mainly useful for debugging purposes.
Reference should be made to the appropriate datasheet for the network card for details on the register addresses
and fields that can be accessed.

read reg
~~~~~~~~

Display the value of a port register:

read reg (port_id) (address)

For example, to examine the Flow Director control register (FDIRCTL, 0x0000EE000) on an Intel® 82599 10 GbE Controller:

.. code-block:: console

    testpmd> read reg 0 0xEE00
    port 0 PCI register at offset 0xEE00: 0x4A060029 (1241907241)

read regfield
~~~~~~~~~~~~~

Display a port register bit field:

read regfield (port_id) (address) (bit_x) (bit_y)

For example, reading the lowest two bits from the register in the example above:

.. code-block:: console

    testpmd> read regfield 0 0xEE00 0 1
    port 0 PCI register at offset 0xEE00: bits[0, 1]=0x1 (1)

read regbit
~~~~~~~~~~~

Display a single port register bit:

read regbit (port_id) (address) (bit_x)

For example, reading the lowest bit from the register in the example above:

.. code-block:: console

    testpmd> read regbit 0 0xEE00 0
    port 0 PCI register at offset 0xEE00: bit 0=1

write reg
~~~~~~~~~

Set the value of a port register:

write reg (port_id) (address) (value)

For example, to clear a register:

.. code-block:: console

    testpmd> write reg 0 0xEE00 0x0
    port 0 PCI register at offset 0xEE00: 0x00000000 (0)

write regfield
~~~~~~~~~~~~~~

Set bit field of a port register:

write regfield (port_id) (address) (bit_x) (bit_y) (value)

For example, writing to the register cleared in the example above:

.. code-block:: console

    testpmd> write regfield 0 0xEE00 0 1 2
    port 0 PCI register at offset 0xEE00: 0x00000002 (2)

write regbit
~~~~~~~~~~~~

Set single bit value of a port register:

write regbit (port_id) (address) (bit_x) (value)

For example, to set the high bit in the register from the example above:

.. code-block:: console

    testpmd> write regbit 0 0xEE00 31 1
    port 0 PCI register at offset 0xEE00: 0x8000000A (2147483658)

Filter Functions
----------------

This section details the available filter functions that are available.

add_ethertype_filter
~~~~~~~~~~~~~~~~~~~~

Add a L2 Ethertype filter, which identify packets by their L2 Ethertype mainly assign them to a receive queue.

add_ethertype_filter (port_id) ethertype (eth_value) priority (enable|disable) (pri_value) queue (queue_id) index (idx)

The available information parameters are:

*   port_id:  the port which the Ethertype filter assigned on.

*   eth_value: the EtherType value want to match,
    for example 0x0806 for ARP packet. 0x0800 (IPv4) and 0x86DD (IPv6) are invalid.

*   enable: user priority participates in the match.

*   disable: user priority doesn't participate in the match.

*   pri_value: user priority value that want to match.

*   queue_id : The receive queue associated with this EtherType filter

*   index: the index of this EtherType filter

Example:

.. code-block:: console

    testpmd> add_ethertype_filter 0 ethertype 0x0806 priority disable 0 queue 3 index 0
    Assign ARP packet to receive queue 3

remove_ethertype_filter
~~~~~~~~~~~~~~~~~~~~~~~

Remove a L2 Ethertype filter

remove_ethertype_filter (port_id) index (idx)

get_ethertype_filter
~~~~~~~~~~~~~~~~~~~~

Get and display a L2 Ethertype filter

get_ethertype_filter (port_id) index (idx)

Example:

.. code-block:: console

    testpmd> get_ethertype_filter 0 index 0

    filter[0]:
        ethertype: 0x0806
        priority: disable, 0
        queue: 3

add_2tuple_filter
~~~~~~~~~~~~~~~~~

Add a 2-tuple filter,
which identify packets by specific protocol and destination TCP/UDP port
and forwards packets into one of the receive queues.

add_2tuple_filter (port_id) protocol (pro_value) (pro_mask) dst_port (port_value) (port_mask)
flags (flg_value) priority (prio_value) queue (queue_id) index (idx)

The available information parameters are:

*   port_id: the port which the 2-tuple filter assigned on.

*   pro_value: IP L4 protocol

*   pro_mask: protocol participates in the match or not, 1 means participate

*   port_value: destination port in L4.

*   port_mask: destination port participates in the match or not, 1 means participate.

*   flg_value: TCP control bits. The non-zero value is invalid, when the pro_value is not set to 0x06 (TCP).

*   prio_value: the priority of this filter.

*   queue_id: The receive queue associated with this 2-tuple filter

*   index: the index of this 2-tuple filter

Example:

.. code-block:: console

    testpmd> add_2tuple_filter 0 protocol 0x06 1 dst_port 32 1 flags 0x02 priority 3 queue 3 index 0

remove_2tuple_filter
~~~~~~~~~~~~~~~~~~~~

Remove a 2-tuple filter

remove_2tuple_filter (port_id) index (idx)

get_2tuple_filter
~~~~~~~~~~~~~~~~~

Get and display a 2-tuple filter

get_2tuple_filter (port_id) index (idx)

Example:

.. code-block:: console

    testpmd> get_2tuple_filter 0 index 0

    filter[0]:
        Destination Port: 0x0020 mask: 1
        protocol: 0x06 mask:1 tcp_flags: 0x02
        priority: 3   queue: 3

add_5tuple_filter
~~~~~~~~~~~~~~~~~

Add a 5-tuple filter,
which consists of a 5-tuple (protocol, source and destination IP addresses, source and destination TCP/UDP/SCTP port)
and routes packets into one of the receive queues.

add_5tuple_filter (port_id) dst_ip (dst_address) src_ip (src_address) dst_port (dst_port_value) src_port (src_port_value)
protocol (protocol_value) mask (mask_value) flags (flags_value) priority (prio_value) queue (queue_id) index (idx)

The available information parameters are:

*   port_id: the port which the 5-tuple filter assigned on.

*   dst_address: destination IP address.

*   src_address: source IP address.

*   dst_port_value: TCP/UDP destination port.

*   src_port_value: TCP/UDP source port.

*   protocol_value: L4 protocol.

*   mask_value: participates in the match or not by bit for field above, 1b means participate

*   flags_value: TCP control bits. The non-zero value is invalid, when the protocol_value is not set to 0x06 (TCP).

*   prio_value: the priority of this filter.

*   queue_id: The receive queue associated with this 5-tuple filter.

*   index: the index of this 5-tuple filter

Example:

.. code-block:: console

    testpmd> add_5tuple_filter 1 dst_ip 2.2.2.5 src_ip 2.2.2.4 dst_port 64 src_port 32 protocol 0x06 mask 0x1F flags 0x0 priority 3 queue 3 index 0

remove_5tuple_filter
~~~~~~~~~~~~~~~~~~~~

Remove a 5-tuple filter

remove_5tuple_filter (port_id) index (idx)

get_5tuple_filter
~~~~~~~~~~~~~~~~~

Get and display a 5-tuple filter

get_5tuple_filter (port_id) index (idx)

Example:

.. code-block:: console

    testpmd> get_5tuple_filter 1 index 0

    filter[0]:
        Destination IP: 0x02020205 mask: 1
        Source IP: 0x02020204 mask: 1
        Destination Port: 0x0040 mask: 1
        Source Port: 0x0020 mask: 1
        protocol: 0x06 mask: 1
        priority: 3 flags: 0x00 queue: 3

add_syn_filter
~~~~~~~~~~~~~~

Add SYN filter, which can forward TCP packets whose *SYN* flag is set into a separate queue.

add_syn_filter (port_id) priority (high|low) queue (queue_id)

The available information parameters are:

*   port_id: the port which the SYN filter assigned on.

*   high: this SYN filter has higher priority than other filters.

*   low: this SYN filter has lower priority than other filters.

*   queue_id: The receive queue associated with this SYN filter

Example:

.. code-block:: console

    testpmd> add_syn_filter 0 priority high queue 3,

remove_syn_filter
~~~~~~~~~~~~~~~~~

Remove SYN filter

remove_syn_filter (port_id)

get_syn_filter
~~~~~~~~~~~~~~

Get and display SYN filter

get_syn_filter (port_id)

Example:

.. code-block:: console

    testpmd> get_syn_filter 0

    syn filter: on, priority: high, queue: 3

add_flex_filter
~~~~~~~~~~~~~~~

Add a Flex filter,
which recognizes any arbitrary pattern within the first 128 bytes of the packet
and routes packets into one of the receive queues.

add_flex_filter (port_id) len (len_value) bytes (bytes_string) mask (mask_value)
priority (prio_value) queue (queue_id) index (idx)

The available information parameters are:

*   port_id: the port which the Flex filter assigned on.

*   len_value: filter length in byte, no greater than 128.

*   bytes_string: a sting in format of octal, means the value the flex filter need to match.

*   mask_value: a sting in format of octal, bit 1 means corresponding byte in DWORD participates in the match.

*   prio_value: the priority of this filter.

*   queue_id: The receive queue associated with this Flex filter.

*   index: the index of this Flex filter

Example:

.. code-block:: console

   testpmd> add_flex_filter 0 len 16 bytes 0x00000000000000000000000008060000 mask 000C priority 3 queue 3 index 0

Assign a packet whose 13th and 14th bytes are 0x0806 to queue 3.

remove_flex_filter
~~~~~~~~~~~~~~~~~~

Remove a Flex filter

remove_flex_filter (port_id) index (idx)

get_flex_filter
~~~~~~~~~~~~~~~

Get and display a Flex filter

get_flex_filter (port_id) index (idx)

Example:

.. code-block:: console

    testpmd> get_flex_filter 0 index 0

    filter[0]:

        length: 16

        dword[]: 0x00000000 00000000 00000000 08060000 00000000 00000000 00000000
    00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    00000000 00000000 00000000 00000000 00000000 00000000 00000000

        mask[]:
    0b0000000000001100000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000

        priority: 3   queue: 3
