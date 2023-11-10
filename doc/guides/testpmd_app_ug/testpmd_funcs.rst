..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2016 Intel Corporation.

.. _testpmd_runtime:

Testpmd Runtime Functions
=========================

Where the testpmd application is started in interactive mode, (``-i|--interactive``),
it displays a prompt that can be used to start and stop forwarding,
configure the application, display statistics (including the extended NIC
statistics aka xstats) , set the Flow Director and other tasks::

   testpmd>

The testpmd prompt has some, limited, readline support.
Common bash command-line functions such as ``Ctrl+a`` and ``Ctrl+e`` to go to the start and end of the prompt line are supported
as well as access to the command history via the up-arrow.

There is also support for tab completion.
If you type a partial command and hit ``<TAB>`` you get a list of the available completions:

.. code-block:: console

   testpmd> show port <TAB>

       info [Mul-choice STRING]: show|clear port info|stats|xstats|fdir|dcb_tc|cap X
       info [Mul-choice STRING]: show|clear port info|stats|xstats|fdir|dcb_tc|cap all
       stats [Mul-choice STRING]: show|clear port info|stats|xstats|fdir|dcb_tc|cap X
       stats [Mul-choice STRING]: show|clear port info|stats|xstats|fdir|dcb_tc|cap all
       ...


.. note::

   Some examples in this document are too long to fit on one line are shown wrapped at `"\\"` for display purposes::

      testpmd> set flow_ctrl rx (on|off) tx (on|off) (high_water) (low_water) \
               (pause_time) (send_xon) (port_id)

In the real ``testpmd>`` prompt these commands should be on a single line.

Help Functions
--------------

The testpmd has on-line help for the functions that are available at runtime.
These are divided into sections and can be accessed using help, help section or help all:

.. code-block:: console

   testpmd> help
       Help is available for the following sections:

           help control                    : Start and stop forwarding.
           help display                    : Displaying port, stats and config information.
           help config                     : Configuration information.
           help ports                      : Configuring ports.
           help filters                    : Filters configuration help.
           help traffic_management         : Traffic Management commands.
           help devices                    : Device related commands.
           help drivers                    : Driver specific commands.
           help all                        : All of the above sections.

Command File Functions
----------------------

To facilitate loading large number of commands or to avoid cutting and pasting where not
practical or possible testpmd supports alternative methods for executing commands.

* If started with the ``--cmdline-file=FILENAME`` command line argument testpmd
  will execute all CLI commands contained within the file immediately before
  starting packet forwarding or entering interactive mode.

.. code-block:: console

   ./dpdk-testpmd -n4 -r2 ... -- -i --cmdline-file=/home/ubuntu/flow-create-commands.txt
   Interactive-mode selected
   CLI commands to be read from /home/ubuntu/flow-create-commands.txt
   Configuring Port 0 (socket 0)
   Port 0: 7C:FE:90:CB:74:CE
   Configuring Port 1 (socket 0)
   Port 1: 7C:FE:90:CB:74:CA
   Checking link statuses...
   Port 0 Link Up - speed 10000 Mbps - full-duplex
   Port 1 Link Up - speed 10000 Mbps - full-duplex
   Done
   Flow rule #0 created
   Flow rule #1 created
   ...
   ...
   Flow rule #498 created
   Flow rule #499 created
   Read all CLI commands from /home/ubuntu/flow-create-commands.txt
   testpmd>


* At run-time additional commands can be loaded in bulk by invoking the ``load FILENAME``
  command.

.. code-block:: console

   testpmd> load /home/ubuntu/flow-create-commands.txt
   Flow rule #0 created
   Flow rule #1 created
   ...
   ...
   Flow rule #498 created
   Flow rule #499 created
   Read all CLI commands from /home/ubuntu/flow-create-commands.txt
   testpmd>


In all cases output from any included command will be displayed as standard output.
Execution will continue until the end of the file is reached regardless of
whether any errors occur.  The end user must examine the output to determine if
any failures occurred.


Control Functions
-----------------

start
~~~~~

Start packet forwarding with current configuration::

   testpmd> start

start tx_first
~~~~~~~~~~~~~~

Start packet forwarding with current configuration after sending specified number of bursts of packets::

   testpmd> start tx_first (""|burst_num)

The default burst number is 1 when ``burst_num`` not presented.

stop
~~~~

Stop packet forwarding, and display accumulated statistics::

   testpmd> stop

quit
~~~~

Quit to prompt::

   testpmd> quit


Display Functions
-----------------

The functions in the following sections are used to display information about the
testpmd configuration or the NIC status.

show port
~~~~~~~~~

Display information for a given port or all ports::

   testpmd> show port (info|summary|stats|xstats|fdir|dcb_tc|cap) (port_id|all)

The available information categories are:

* ``info``: General port information such as MAC address.

* ``summary``: Brief port summary such as Device Name, Driver Name etc.

* ``stats``: RX/TX statistics.

* ``xstats``: RX/TX extended NIC statistics.

* ``fdir``: Flow Director information and statistics.

* ``dcb_tc``: DCB information such as TC mapping.

For example:

.. code-block:: console

   testpmd> show port info 0

   ********************* Infos for port 0 *********************

   MAC address: XX:XX:XX:XX:XX:XX
   Connect to socket: 0
   memory allocation on the socket: 0
   Link status: up
   Link speed: 40000 Mbps
   Link duplex: full-duplex
   Promiscuous mode: enabled
   Allmulticast mode: disabled
   Maximum number of MAC addresses: 64
   Maximum number of MAC addresses of hash filtering: 0
   VLAN offload:
       strip on, filter on, extend off, qinq strip off
   Redirection table size: 512
   Supported flow types:
     ipv4-frag
     ipv4-tcp
     ipv4-udp
     ipv4-sctp
     ipv4-other
     ipv6-frag
     ipv6-tcp
     ipv6-udp
     ipv6-sctp
     ipv6-other
     l2_payload
     port
     vxlan
     geneve
     nvgre
     vxlan-gpe

show port (module_eeprom|eeprom)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Display the EEPROM information of a port::

   testpmd> show port (port_id) (module_eeprom|eeprom)

show port rss reta
~~~~~~~~~~~~~~~~~~

Display the rss redirection table entry indicated by masks on port X::

   testpmd> show port (port_id) rss reta (size) (mask0, mask1...)

size is used to indicate the hardware supported reta size

show port rss-hash
~~~~~~~~~~~~~~~~~~

Display the RSS hash functions and RSS hash key or RSS hash algorithm of a port::

   testpmd> show port (port_id) rss-hash [key | algorithm]

clear port
~~~~~~~~~~

Clear the port statistics and forward engine statistics for a given port or for all ports::

   testpmd> clear port (info|stats|xstats|fdir) (port_id|all)

For example::

   testpmd> clear port stats all

show (rxq|txq)
~~~~~~~~~~~~~~

Display information for a given port's RX/TX queue::

   testpmd> show (rxq|txq) info (port_id) (queue_id)

show desc status(rxq|txq)
~~~~~~~~~~~~~~~~~~~~~~~~~

Display information for a given port's RX/TX descriptor status::

   testpmd> show port (port_id) (rxq|txq) (queue_id) desc (desc_id) status

show rxq desc used count
~~~~~~~~~~~~~~~~~~~~~~~~

Display the number of receive packet descriptors currently filled by hardware
and ready to be processed by the driver on a given RX queue::

   testpmd> show port (port_id) rxq (queue_id) desc used count

show config
~~~~~~~~~~~

Displays the configuration of the application.
The configuration comes from the command-line, the runtime or the application defaults::

   testpmd> show config (rxtx|cores|fwd|rxoffs|rxpkts|rxhdrs|txpkts|txtimes)

The available information categories are:

* ``rxtx``: RX/TX configuration items.

* ``cores``: List of forwarding cores.

* ``fwd``: Packet forwarding configuration.

* ``rxoffs``: Packet offsets for RX split.

* ``rxpkts``: Packets to RX length-based split configuration.

* ``rxhdrs``: Packets to RX proto-based split configuration.

* ``txpkts``: Packets to TX configuration.

* ``txtimes``: Burst time pattern for Tx only mode.

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

set fwd
~~~~~~~

Set the packet forwarding mode::

   testpmd> set fwd (io|mac|macswap|flowgen| \
                     rxonly|txonly|csum|icmpecho|noisy|5tswap|shared-rxq|recycle_mbufs) (""|retry)

``retry`` can be specified for forwarding engines except ``rx_only``.

The available information categories are:

* ``io``: Forwards packets "as-is" in I/O mode.
  This is the fastest possible forwarding operation as it does not access packets data.
  This is the default mode.

* ``mac``: Changes the source and the destination Ethernet addresses of packets before forwarding them.
  Default application behavior is to set source Ethernet address to that of the transmitting interface, and destination
  address to a dummy value (set during init). The user may specify a target destination Ethernet address via the 'eth-peer' or
  'eth-peers-configfile' command-line options. It is not currently possible to specify a specific source Ethernet address.

* ``macswap``: MAC swap forwarding mode.
  Swaps the source and the destination Ethernet addresses of packets before forwarding them.

* ``flowgen``: Multi-flow generation mode.
  Originates a number of flows (with varying destination IP addresses), and terminate receive traffic.

* ``rxonly``: Receives packets but doesn't transmit them.

* ``txonly``: Generates and transmits packets without receiving any.

* ``csum``: Changes the checksum field with hardware or software methods depending on the offload flags on the packet.

* ``icmpecho``: Receives a burst of packets, lookup for ICMP echo requests and, if any, send back ICMP echo replies.

* ``ieee1588``: Demonstrate L2 IEEE1588 V2 PTP timestamping for RX and TX.

* ``noisy``: Noisy neighbor simulation.
  Simulate more realistic behavior of a guest machine engaged in receiving
  and sending packets performing Virtual Network Function (VNF).

* ``5tswap``: Swap the source and destination of L2,L3,L4 if they exist.

  L2 swaps the source address and destination address of Ethernet, as same as ``macswap``.

  L3 swaps the source address and destination address of IP (v4 and v6).

  L4 swaps the source port and destination port of transport layer (TCP and UDP).

* ``shared-rxq``: Receive only for shared Rx queue.
  Resolve packet source port from mbuf and update stream statistics accordingly.

* ``recycle_mbufs``:  Recycle Tx queue used mbufs for Rx queue mbuf ring.
  This mode uses fast path mbuf recycle feature and forwards packets in I/O mode.

Example::

   testpmd> set fwd rxonly

   Set rxonly packet forwarding mode


show fwd
~~~~~~~~

When running, forwarding engines maintain statistics from the time they have been started.
Example for the io forwarding engine, with some packet drops on the tx side::

   testpmd> show fwd stats all

     ------- Forward Stats for RX Port= 0/Queue= 0 -> TX Port= 1/Queue= 0 -------
     RX-packets: 274293770      TX-packets: 274293642      TX-dropped: 128

     ------- Forward Stats for RX Port= 1/Queue= 0 -> TX Port= 0/Queue= 0 -------
     RX-packets: 274301850      TX-packets: 274301850      TX-dropped: 0

     ---------------------- Forward statistics for port 0  ----------------------
     RX-packets: 274293802      RX-dropped: 0             RX-total: 274293802
     TX-packets: 274301862      TX-dropped: 0             TX-total: 274301862
     ----------------------------------------------------------------------------

     ---------------------- Forward statistics for port 1  ----------------------
     RX-packets: 274301894      RX-dropped: 0             RX-total: 274301894
     TX-packets: 274293706      TX-dropped: 128           TX-total: 274293834
     ----------------------------------------------------------------------------

     +++++++++++++++ Accumulated forward statistics for all ports+++++++++++++++
     RX-packets: 548595696      RX-dropped: 0             RX-total: 548595696
     TX-packets: 548595568      TX-dropped: 128           TX-total: 548595696
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


clear fwd
~~~~~~~~~

Clear the forwarding engines statistics::

   testpmd> clear fwd stats all

read rxd
~~~~~~~~

Display an RX descriptor for a port RX queue::

   testpmd> read rxd (port_id) (queue_id) (rxd_id)

For example::

   testpmd> read rxd 0 0 4
        0x0000000B - 0x001D0180 / 0x0000000B - 0x001D0180

read txd
~~~~~~~~

Display a TX descriptor for a port TX queue::

   testpmd> read txd (port_id) (queue_id) (txd_id)

For example::

   testpmd> read txd 0 0 4
        0x00000001 - 0x24C3C440 / 0x000F0000 - 0x2330003C

show vf stats
~~~~~~~~~~~~~

Display VF statistics::

   testpmd> show vf stats (port_id) (vf_id)

clear vf stats
~~~~~~~~~~~~~~

Reset VF statistics::

   testpmd> clear vf stats (port_id) (vf_id)

show rx offloading capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

List all per queue and per port Rx offloading capabilities of a port::

   testpmd> show port (port_id) rx_offload capabilities

show rx offloading configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

List port level and all queue level Rx offloading configuration::

   testpmd> show port (port_id) rx_offload configuration

show tx offloading capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

List all per queue and per port Tx offloading capabilities of a port::

   testpmd> show port (port_id) tx_offload capabilities

show tx offloading configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

List port level and all queue level Tx offloading configuration::

   testpmd> show port (port_id) tx_offload configuration

show tx metadata setting
~~~~~~~~~~~~~~~~~~~~~~~~

Show Tx metadata value set for a specific port::

   testpmd> show port (port_id) tx_metadata

show port supported ptypes
~~~~~~~~~~~~~~~~~~~~~~~~~~

Show ptypes supported for a specific port::

   testpmd> show port (port_id) ptypes

set port supported ptypes
~~~~~~~~~~~~~~~~~~~~~~~~~

set packet types classification for a specific port::

   testpmd> set port (port_id) ptypes_mask (mask)

show port mac addresses info
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show mac addresses added for a specific port::

   testpmd> show port (port_id) macs


show port multicast mac addresses info
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show multicast mac addresses added for a specific port::

   testpmd> show port (port_id) mcast_macs

show flow transfer proxy port ID for the given port
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show proxy port ID to use as the 1st argument in commands to
manage ``transfer`` flows and their indirect components.
::

   testpmd> show port (port_id) flow transfer proxy

show device info
~~~~~~~~~~~~~~~~

Show general information about devices probed::

   testpmd> show device info (<identifier>|all)

For example:

.. code-block:: console

    testpmd> show device info net_pcap0

    ********************* Infos for device net_pcap0 *********************
    Bus name: vdev
    Driver name: net_pcap
    Devargs: iface=enP2p6s0,phy_mac=1
    Connect to socket: -1

            Port id: 2
            MAC address: 1E:37:93:28:04:B8
            Device name: net_pcap0

dump physmem
~~~~~~~~~~~~

Dumps all physical memory segment layouts::

   testpmd> dump_physmem

dump memzone
~~~~~~~~~~~~

Dumps the layout of all memory zones::

   testpmd> dump_memzone

dump socket memory
~~~~~~~~~~~~~~~~~~

Dumps the memory usage of all sockets::

   testpmd> dump_socket_mem

dump struct size
~~~~~~~~~~~~~~~~

Dumps the size of all memory structures::

   testpmd> dump_struct_sizes

dump ring
~~~~~~~~~

Dumps the status of all or specific element in DPDK rings::

   testpmd> dump_ring [ring_name]

dump mempool
~~~~~~~~~~~~

Dumps the statistics of all or specific memory pool::

   testpmd> dump_mempool [mempool_name]

dump devargs
~~~~~~~~~~~~

Dumps the user device list::

   testpmd> dump_devargs

dump lcores
~~~~~~~~~~~

Dumps the logical cores list::

   testpmd> dump_lcores

dump trace
~~~~~~~~~~

Dumps the tracing data to the folder according to the current EAL settings::

   testpmd> dump_trace

dump log types
~~~~~~~~~~~~~~

Dumps the log level for all the dpdk modules::

   testpmd> dump_log_types

show (raw_encap|raw_decap)
~~~~~~~~~~~~~~~~~~~~~~~~~~

Display content of raw_encap/raw_decap buffers in hex::

  testpmd> show <raw_encap|raw_decap> <index>
  testpmd> show <raw_encap|raw_decap> all

For example::

  testpmd> show raw_encap 6

  index: 6 at [0x1c565b0], len=50
  00000000: 00 00 00 00 00 00 16 26 36 46 56 66 08 00 45 00 | .......&6FVf..E.
  00000010: 00 00 00 00 00 00 00 11 00 00 C0 A8 01 06 C0 A8 | ................
  00000020: 03 06 00 00 00 FA 00 00 00 00 08 00 00 00 00 00 | ................
  00000030: 06 00                                           | ..

show fec capabilities
~~~~~~~~~~~~~~~~~~~~~

Show fec capabilities of a port::

  testpmd> show port (port_id) fec capabilities

show fec mode
~~~~~~~~~~~~~

Show fec mode of a port::

  testpmd> show port (port_id) fec_mode


Configuration Functions
-----------------------

The testpmd application can be configured from the runtime as well as from the command-line.

This section details the available configuration functions that are available.

.. note::

   Configuration changes only become active when forwarding is started/restarted.

set default
~~~~~~~~~~~

Reset forwarding to the default configuration::

   testpmd> set default

set verbose
~~~~~~~~~~~

Set the debug verbosity level::

   testpmd> set verbose (level)

Available levels are as following:

* ``0`` silent except for error.
* ``1`` fully verbose except for Tx packets.
* ``2`` fully verbose except for Rx packets.
* ``> 2`` fully verbose.

set log
~~~~~~~

Set the log level for a log type::

	testpmd> set log global|(type) (level)

Where:

* ``type`` is the log name.

* ``level`` is the log level.

For example, to change the global log level::

	testpmd> set log global (level)

Regexes can also be used for type. To change log level of user1, user2 and user3::

	testpmd> set log user[1-3] (level)

set nbport
~~~~~~~~~~

Set the number of ports used by the application:

set nbport (num)

This is equivalent to the ``--nb-ports`` command-line option.

set nbcore
~~~~~~~~~~

Set the number of cores used by the application::

   testpmd> set nbcore (num)

This is equivalent to the ``--nb-cores`` command-line option.

.. note::

   The number of cores used must not be greater than number of ports used multiplied by the number of queues per port.

set coremask
~~~~~~~~~~~~

Set the forwarding cores hexadecimal mask::

   testpmd> set coremask (mask)

This is equivalent to the ``--coremask`` command-line option.

.. note::

   The main lcore is reserved for command line parsing only and cannot be masked on for packet forwarding.

set portmask
~~~~~~~~~~~~

Set the forwarding ports hexadecimal mask::

   testpmd> set portmask (mask)

This is equivalent to the ``--portmask`` command-line option.

set record-core-cycles
~~~~~~~~~~~~~~~~~~~~~~

Set the recording of CPU cycles::

   testpmd> set record-core-cycles (on|off)

Where:

* ``on`` enables measurement of CPU cycles per packet.

* ``off`` disables measurement of CPU cycles per packet.

This is equivalent to the ``--record-core-cycles command-line`` option.

set record-burst-stats
~~~~~~~~~~~~~~~~~~~~~~

Set the displaying of RX and TX bursts::

   testpmd> set record-burst-stats (on|off)

Where:

* ``on`` enables display of RX and TX bursts.

* ``off`` disables display of RX and TX bursts.

This is equivalent to the ``--record-burst-stats command-line`` option.

set burst
~~~~~~~~~

Set number of packets per burst::

   testpmd> set burst (num)

This is equivalent to the ``--burst command-line`` option.

When retry is enabled, the transmit delay time and number of retries can also be set::

   testpmd> set burst tx delay (microseconds) retry (num)

set rxoffs
~~~~~~~~~~

Set the offsets of segments relating to the data buffer beginning on receiving
if split feature is engaged. Affects only the queues configured with split
offloads (currently BUFFER_SPLIT is supported only).

   testpmd> set rxoffs (x[,y]*)

Where x[,y]* represents a CSV list of values, without white space. If the list
of offsets is shorter than the list of segments the zero offsets will be used
for the remaining segments.

set rxpkts
~~~~~~~~~~

Set the length of segments to scatter packets on receiving if split
feature is engaged. Affects only the queues configured with split offloads
(currently BUFFER_SPLIT is supported only). Optionally the multiple memory
pools can be specified with --mbuf-size command line parameter and the mbufs
to receive will be allocated sequentially from these extra memory pools (the
mbuf for the first segment is allocated from the first pool, the second one
from the second pool, and so on, if segment number is greater then pool's the
mbuf for remaining segments will be allocated from the last valid pool).

   testpmd> set rxpkts (x[,y]*)

Where x[,y]* represents a CSV list of values, without white space. Zero value
means to use the corresponding memory pool data buffer size.

set rxhdrs
~~~~~~~~~~

Set the protocol headers of segments to scatter packets on receiving
if split feature is engaged.
Affects only the queues configured with split offloads
(currently BUFFER_SPLIT is supported only).

   testpmd> set rxhdrs (eth[,ipv4]*)

Where eth[,ipv4]* represents a CSV list of values, without white space.
If the list of offsets is shorter than the list of segments,
zero offsets will be used for the remaining segments.

set txpkts
~~~~~~~~~~

Set the length of each segment of the TX-ONLY packets or length of packet for FLOWGEN mode::

   testpmd> set txpkts (x[,y]*)

Where x[,y]* represents a CSV list of values, without white space.

set txtimes
~~~~~~~~~~~

Configure the timing burst pattern for Tx only mode. This command enables
the packet send scheduling on dynamic timestamp mbuf field and configures
timing pattern in Tx only mode. In this mode, if scheduling is enabled
application provides timestamps in the packets being sent. It is possible
to configure delay (in unspecified device clock units) between bursts
and between the packets within the burst::

   testpmd> set txtimes (inter),(intra)

where:

* ``inter``  is the delay between the bursts in the device clock units.
  If ``intra`` is zero, this is the time between the beginnings of the
  first packets in the neighbour bursts, if ``intra`` is not zero,
  ``inter`` specifies the time between the beginning of the first packet
  of the current burst and the beginning of the last packet of the
  previous burst. If ``inter`` parameter is zero the send scheduling
  on timestamps is disabled (default).

* ``intra`` is the delay between the packets within the burst specified
  in the device clock units. The number of packets in the burst is defined
  by regular burst setting. If ``intra`` parameter is zero no timestamps
  provided in the packets excepting the first one in the burst.

As the result the bursts of packet will be transmitted with specific
delays between the packets within the burst and specific delay between
the bursts. The rte_eth_read_clock() must be supported by the device(s)
and is supposed to be engaged to get the current device clock value
and provide the reference for the timestamps. If there is no supported
rte_eth_read_clock() there will be no send scheduling provided on the port.

set txsplit
~~~~~~~~~~~

Set the split policy for the TX packets, applicable for TX-ONLY and CSUM forwarding modes::

   testpmd> set txsplit (off|on|rand)

Where:

* ``off`` disable packet copy & split for CSUM mode.

* ``on`` split outgoing packet into multiple segments. Size of each segment
  and number of segments per packet is determined by ``set txpkts`` command
  (see above).

* ``rand`` same as 'on', but number of segments per each packet is a random value between 1 and total number of segments.

set corelist
~~~~~~~~~~~~

Set the list of forwarding cores::

   testpmd> set corelist (x[,y]*)

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

Set the list of forwarding ports::

   testpmd> set portlist (x[,y]*)

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

set port setup on
~~~~~~~~~~~~~~~~~

Select how to retrieve new ports created after "port attach" command::

   testpmd> set port setup on (iterator|event)

For each new port, a setup is done.
It will find the probed ports via RTE_ETH_FOREACH_MATCHING_DEV loop
in iterator mode, or via RTE_ETH_EVENT_NEW in event mode.

set tx loopback
~~~~~~~~~~~~~~~

Enable/disable tx loopback::

   testpmd> set tx loopback (port_id) (on|off)

set drop enable
~~~~~~~~~~~~~~~

set drop enable bit for all queues::

   testpmd> set all queues drop (port_id) (on|off)

set mac antispoof (for VF)
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set mac antispoof for a VF from the PF::

   testpmd> set vf mac antispoof  (port_id) (vf_id) (on|off)

vlan set stripq
~~~~~~~~~~~~~~~

Set the VLAN strip for a queue on a port::

   testpmd> vlan set stripq (on|off) (port_id,queue_id)

vlan set stripq (for VF)
~~~~~~~~~~~~~~~~~~~~~~~~

Set VLAN strip for all queues in a pool for a VF from the PF::

   testpmd> set vf vlan stripq (port_id) (vf_id) (on|off)

vlan set insert (for VF)
~~~~~~~~~~~~~~~~~~~~~~~~

Set VLAN insert for a VF from the PF::

   testpmd> set vf vlan insert (port_id) (vf_id) (vlan_id)

vlan set antispoof (for VF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set VLAN antispoof for a VF from the PF::

   testpmd> set vf vlan antispoof (port_id) (vf_id) (on|off)

vlan set (strip|filter|qinq_strip|extend)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Set the VLAN strip/filter/QinQ strip/extend on for a port::

   testpmd> vlan set (strip|filter|qinq_strip|extend) (on|off) (port_id)

vlan set tpid
~~~~~~~~~~~~~

Set the inner or outer VLAN TPID for packet filtering on a port::

   testpmd> vlan set (inner|outer) tpid (value) (port_id)

.. note::

   TPID value must be a 16-bit number (value <= 65536).

rx_vlan add
~~~~~~~~~~~

Add a VLAN ID, or all identifiers, to the set of VLAN identifiers filtered by port ID::

   testpmd> rx_vlan add (vlan_id|all) (port_id)

.. note::

   VLAN filter must be set on that port. VLAN ID < 4096.
   Depending on the NIC used, number of vlan_ids may be limited to the maximum entries
   in VFTA table. This is important if enabling all vlan_ids.

rx_vlan rm
~~~~~~~~~~

Remove a VLAN ID, or all identifiers, from the set of VLAN identifiers filtered by port ID::

   testpmd> rx_vlan rm (vlan_id|all) (port_id)

rx_vlan add (for VF)
~~~~~~~~~~~~~~~~~~~~

Add a VLAN ID, to the set of VLAN identifiers filtered for VF(s) for port ID::

   testpmd> rx_vlan add (vlan_id) port (port_id) vf (vf_mask)

rx_vlan rm (for VF)
~~~~~~~~~~~~~~~~~~~

Remove a VLAN ID, from the set of VLAN identifiers filtered for VF(s) for port ID::

   testpmd> rx_vlan rm (vlan_id) port (port_id) vf (vf_mask)

rx_vxlan_port add
~~~~~~~~~~~~~~~~~

Add an UDP port for VXLAN packet filter on a port::

   testpmd> rx_vxlan_port add (udp_port) (port_id)

rx_vxlan_port remove
~~~~~~~~~~~~~~~~~~~~

Remove an UDP port for VXLAN packet filter on a port::

   testpmd> rx_vxlan_port rm (udp_port) (port_id)

tx_vlan set
~~~~~~~~~~~

Set hardware insertion of VLAN IDs in packets sent on a port::

   testpmd> tx_vlan set (port_id) vlan_id[, vlan_id_outer]

For example, set a single VLAN ID (5) insertion on port 0::

   tx_vlan set 0 5

Or, set double VLAN ID (inner: 2, outer: 3) insertion on port 1::

   tx_vlan set 1 2 3


tx_vlan set pvid
~~~~~~~~~~~~~~~~

Set port based hardware insertion of VLAN ID in packets sent on a port::

   testpmd> tx_vlan set pvid (port_id) (vlan_id) (on|off)

tx_vlan reset
~~~~~~~~~~~~~

Disable hardware insertion of a VLAN header in packets sent on a port::

   testpmd> tx_vlan reset (port_id)

csum set
~~~~~~~~

Select hardware or software calculation of the checksum when
transmitting a packet using the ``csum`` forwarding engine::

   testpmd> csum set (ip|udp|tcp|sctp|outer-ip|outer-udp) (hw|sw) (port_id)

Where:

* ``ip|udp|tcp|sctp`` always relate to  the inner layer.

* ``outer-ip`` relates to the outer IP layer (only for IPv4) in the case where the packet is recognized
  as a tunnel packet by the forwarding engine (geneve, gre, gtp, ipip, vxlan and vxlan-gpe are
  supported). See also the ``csum parse-tunnel`` command.

* ``outer-udp`` relates to the outer UDP layer in the case where the packet is recognized
  as a tunnel packet by the forwarding engine (geneve, gtp, vxlan and vxlan-gpe are
  supported). See also the ``csum parse-tunnel`` command.

.. note::

   Check the NIC Datasheet for hardware limits.

csum parse-tunnel
~~~~~~~~~~~~~~~~~

Define how tunneled packets should be handled by the csum forward
engine::

   testpmd> csum parse-tunnel (on|off) (tx_port_id)

If enabled, the csum forward engine will try to recognize supported
tunnel headers (geneve, gtp, gre, ipip, vxlan, vxlan-gpe).

If disabled, treat tunnel packets as non-tunneled packets (a inner
header is handled as a packet payload).

.. note::

   The port argument is the TX port like in the ``csum set`` command.

Example:

Consider a packet in packet like the following::

   eth_out/ipv4_out/udp_out/vxlan/eth_in/ipv4_in/tcp_in

* If parse-tunnel is enabled, the ``ip|udp|tcp|sctp`` parameters of ``csum set``
  command relate to the inner headers (here ``ipv4_in`` and ``tcp_in``), and the
  ``outer-ip|outer-udp`` parameter relates to the outer headers (here ``ipv4_out`` and ``udp_out``).

* If parse-tunnel is disabled, the ``ip|udp|tcp|sctp`` parameters of ``csum  set``
   command relate to the outer headers, here ``ipv4_out`` and ``udp_out``.

csum show
~~~~~~~~~

Display tx checksum offload configuration::

   testpmd> csum show (port_id)

tso set
~~~~~~~

Enable TCP Segmentation Offload (TSO) in the ``csum`` forwarding engine::

   testpmd> tso set (segsize) (port_id)

.. note::

   Check the NIC datasheet for hardware limits.

tso show
~~~~~~~~

Display the status of TCP Segmentation Offload::

   testpmd> tso show (port_id)

tunnel tso set
~~~~~~~~~~~~~~

Set tso segment size of tunneled packets for a port in csum engine::

   testpmd> tunnel_tso set (tso_segsz) (port_id)

tunnel tso show
~~~~~~~~~~~~~~~

Display the status of tunneled TCP Segmentation Offload for a port::

   testpmd> tunnel_tso show (port_id)

set port - gro
~~~~~~~~~~~~~~

Enable or disable GRO in ``csum`` forwarding engine::

   testpmd> set port <port_id> gro on|off

If enabled, the csum forwarding engine will perform GRO on the TCP/IPv4
packets received from the given port.

If disabled, packets received from the given port won't be performed
GRO. By default, GRO is disabled for all ports.

.. note::

   When enable GRO for a port, TCP/IPv4 packets received from the port
   will be performed GRO. After GRO, all merged packets have bad
   checksums, since the GRO library doesn't re-calculate checksums for
   the merged packets. Therefore, if users want the merged packets to
   have correct checksums, please select HW IP checksum calculation and
   HW TCP checksum calculation for the port which the merged packets are
   transmitted to.

show port - gro
~~~~~~~~~~~~~~~

Display GRO configuration for a given port::

   testpmd> show port <port_id> gro

set gro flush
~~~~~~~~~~~~~

Set the cycle to flush the GROed packets from reassembly tables::

   testpmd> set gro flush <cycles>

When enable GRO, the csum forwarding engine performs GRO on received
packets, and the GROed packets are stored in reassembly tables. Users
can use this command to determine when the GROed packets are flushed
from the reassembly tables.

The ``cycles`` is measured in GRO operation times. The csum forwarding
engine flushes the GROed packets from the tables every ``cycles`` GRO
operations.

By default, the value of ``cycles`` is 1, which means flush GROed packets
from the reassembly tables as soon as one GRO operation finishes. The value
of ``cycles`` should be in the range of 1 to ``GRO_MAX_FLUSH_CYCLES``.

Please note that the large value of ``cycles`` may cause the poor TCP/IP
stack performance. Because the GROed packets are delayed to arrive the
stack, thus causing more duplicated ACKs and TCP retransmissions.

set port - gso
~~~~~~~~~~~~~~

Toggle per-port GSO support in ``csum`` forwarding engine::

   testpmd> set port <port_id> gso on|off

If enabled, the csum forwarding engine will perform GSO on supported IPv4
packets, transmitted on the given port.

If disabled, packets transmitted on the given port will not undergo GSO.
By default, GSO is disabled for all ports.

.. note::

   When GSO is enabled on a port, supported IPv4 packets transmitted on that
   port undergo GSO. Afterwards, the segmented packets are represented by
   multi-segment mbufs; however, the csum forwarding engine doesn't calculation
   of checksums for GSO'd segments in SW. As a result, if users want correct
   checksums in GSO segments, they should enable HW checksum calculation for
   GSO-enabled ports.

   For example, HW checksum calculation for VxLAN GSO'd packets may be enabled
   by setting the following options in the csum forwarding engine:

   testpmd> csum set outer_ip hw <port_id>

   testpmd> csum set ip hw <port_id>

   testpmd> csum set tcp hw <port_id>

   UDP GSO is the same as IP fragmentation, which treats the UDP header
   as the payload and does not modify it during segmentation. That is,
   after UDP GSO, only the first output fragment has the original UDP
   header. Therefore, users need to enable HW IP checksum calculation
   and SW UDP checksum calculation for GSO-enabled ports, if they want
   correct checksums for UDP/IPv4 packets.

set gso segsz
~~~~~~~~~~~~~

Set the maximum GSO segment size (measured in bytes), which includes the
packet header and the packet payload for GSO-enabled ports (global)::

   testpmd> set gso segsz <length>

show port - gso
~~~~~~~~~~~~~~~

Display the status of Generic Segmentation Offload for a given port::

   testpmd> show port <port_id> gso

mac_addr add
~~~~~~~~~~~~

Add an alternative MAC address to a port::

   testpmd> mac_addr add (port_id) (XX:XX:XX:XX:XX:XX)

mac_addr remove
~~~~~~~~~~~~~~~

Remove a MAC address from a port::

   testpmd> mac_addr remove (port_id) (XX:XX:XX:XX:XX:XX)

mcast_addr add
~~~~~~~~~~~~~~

To add the multicast MAC address to/from the set of multicast addresses
filtered by port::

   testpmd> mcast_addr add (port_id) (mcast_addr)

mcast_addr remove
~~~~~~~~~~~~~~~~~

To remove the multicast MAC address to/from the set of multicast addresses
filtered by port::

   testpmd> mcast_addr remove (port_id) (mcast_addr)

mcast_addr flush
~~~~~~~~~~~~~~~~

Flush all multicast MAC addresses on port_id::

   testpmd> mcast_addr flush (port_id)

mac_addr add (for VF)
~~~~~~~~~~~~~~~~~~~~~

Add an alternative MAC address for a VF to a port::

   testpmd> mac_add add port (port_id) vf (vf_id) (XX:XX:XX:XX:XX:XX)

mac_addr set
~~~~~~~~~~~~

Set the default MAC address for a port::

   testpmd> mac_addr set (port_id) (XX:XX:XX:XX:XX:XX)

mac_addr set (for VF)
~~~~~~~~~~~~~~~~~~~~~

Set the MAC address for a VF from the PF::

   testpmd> set vf mac addr (port_id) (vf_id) (XX:XX:XX:XX:XX:XX)

set eth-peer
~~~~~~~~~~~~

Set the forwarding peer address for certain port::

   testpmd> set eth-peer (port_id) (peer_addr)

This is equivalent to the ``--eth-peer`` command-line option.

set port-uta
~~~~~~~~~~~~

Set the unicast hash filter(s) on/off for a port::

   testpmd> set port (port_id) uta (XX:XX:XX:XX:XX:XX|all) (on|off)

set promisc
~~~~~~~~~~~

Set the promiscuous mode on for a port or for all ports.
In promiscuous mode packets are not dropped if they aren't for the specified MAC address::

   testpmd> set promisc (port_id|all) (on|off)

set allmulti
~~~~~~~~~~~~

Set the allmulti mode for a port or for all ports::

   testpmd> set allmulti (port_id|all) (on|off)

Same as the ifconfig (8) option. Controls how multicast packets are handled.

set flow_ctrl rx
~~~~~~~~~~~~~~~~

Set the link flow control parameter on a port::

   testpmd> set flow_ctrl rx (on|off) tx (on|off) (high_water) (low_water) \
            (pause_time) (send_xon) mac_ctrl_frame_fwd (on|off) \
	    autoneg (on|off) (port_id)

Where:

* ``high_water`` (integer): High threshold value to trigger XOFF.

* ``low_water`` (integer): Low threshold value to trigger XON.

* ``pause_time`` (integer): Pause quota in the Pause frame.

* ``send_xon`` (0/1): Send XON frame.

* ``mac_ctrl_frame_fwd``: Enable receiving MAC control frames.

* ``autoneg``: Change the auto-negotiation parameter.

show flow control
~~~~~~~~~~~~~~~~~

show the link flow control parameter on a port::

   testpmd> show port <port_id> flow_ctrl

set pfc_ctrl rx
~~~~~~~~~~~~~~~

Set the priority flow control parameter on a port::

   testpmd> set pfc_ctrl rx (on|off) tx (on|off) (high_water) (low_water) \
            (pause_time) (priority) (port_id)

Where:

* ``high_water`` (integer): High threshold value.

* ``low_water`` (integer): Low threshold value.

* ``pause_time`` (integer): Pause quota in the Pause frame.

* ``priority`` (0-7): VLAN User Priority.

set pfc_queue_ctrl
~~~~~~~~~~~~~~~~~~

Set the priority flow control parameter on a given Rx and Tx queue of a port::

   testpmd> set pfc_queue_ctrl <port_id> rx (on|off) <tx_qid> <tx_tc> \
            tx (on|off) <rx_qid> <rx_tc> <pause_time>

Where:

* ``tx_qid`` (integer): Tx qid for which ``tx_tc`` will be applied and traffic
  will be paused when PFC frame is received with ``tx_tc`` enabled.

* ``tx_tc`` (0-15): TC for which traffic is to be paused for xmit.

* ``rx_qid`` (integer): Rx qid for which threshold will be applied and PFC
  frame will be generated with ``tx_tc`` when exceeds the threshold.

* ``rx_tc`` (0-15): TC filled in PFC frame for which remote Tx is to be paused.

* ``pause_time`` (integer): Pause quanta filled in the PFC frame for which
  interval, remote Tx will be paused. Valid only if Tx pause is on.

Set Rx queue available descriptors threshold
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set available descriptors threshold for a specific Rx queue of port::

  testpmd> set port (port_id) rxq (queue_id) avail_thresh (0..99)

Use 0 value to disable the threshold and corresponding event.

set stat_qmap
~~~~~~~~~~~~~

Set statistics mapping (qmapping 0..15) for RX/TX queue on port::

   testpmd> set stat_qmap (tx|rx) (port_id) (queue_id) (qmapping)

For example, to set rx queue 2 on port 0 to mapping 5::

   testpmd>set stat_qmap rx 0 2 5

set xstats-hide-zero
~~~~~~~~~~~~~~~~~~~~

Set the option to hide zero values for xstats display::

	testpmd> set xstats-hide-zero on|off

.. note::

	By default, the zero values are displayed for xstats.

set port - rx/tx (for VF)
~~~~~~~~~~~~~~~~~~~~~~~~~

Set VF receive/transmit from a port::

   testpmd> set port (port_id) vf (vf_id) (rx|tx) (on|off)

set port - rx mode(for VF)
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the VF receive mode of a port::

   testpmd> set port (port_id) vf (vf_id) \
            rxmode (AUPE|ROPE|BAM|MPE) (on|off)

The available receive modes are:

* ``AUPE``: Accepts untagged VLAN.

* ``ROPE``: Accepts unicast hash.

* ``BAM``: Accepts broadcast packets.

* ``MPE``: Accepts all multicast packets.

set port - tx_rate (for Queue)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set TX rate limitation for a queue on a port::

   testpmd> set port (port_id) queue (queue_id) rate (rate_value)

set port - tx_rate (for VF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set TX rate limitation for queues in VF on a port::

   testpmd> set port (port_id) vf (vf_id) rate (rate_value) queue_mask (queue_mask)

set flush_rx
~~~~~~~~~~~~

Set the flush on RX streams before forwarding.
The default is flush ``on``.
Mainly used with PCAP drivers to turn off the default behavior of flushing the first 512 packets on RX streams::

   testpmd> set flush_rx off

set link up
~~~~~~~~~~~

Set link up for a port::

   testpmd> set link-up port (port id)

set link down
~~~~~~~~~~~~~

Set link down for a port::

   testpmd> set link-down port (port id)

E-tag set
~~~~~~~~~

Enable E-tag insertion for a VF on a port::

   testpmd> E-tag set insertion on port-tag-id (value) port (port_id) vf (vf_id)

Disable E-tag insertion for a VF on a port::

   testpmd> E-tag set insertion off port (port_id) vf (vf_id)

Enable/disable E-tag stripping on a port::

   testpmd> E-tag set stripping (on|off) port (port_id)

Enable/disable E-tag based forwarding on a port::

   testpmd> E-tag set forwarding (on|off) port (port_id)

config per port Rx offloading
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enable or disable port Rx offloading on all Rx queues of a port::

   testpmd> port config (port_id|all) rx_offload (offloading) on|off

* ``offloading``: can be any of these offloading capability:
                  all, vlan_strip, ipv4_cksum, udp_cksum, tcp_cksum, tcp_lro,
                  qinq_strip, outer_ipv4_cksum, macsec_strip,
                  vlan_filter, vlan_extend, scatter, timestamp, security,
                  keep_crc, rss_hash

This command should be run when the port is stopped, or else it will fail.

config per queue Rx offloading
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enable or disable a per queue Rx offloading only on a specific Rx queue::

   testpmd> port (port_id) rxq (queue_id) rx_offload (offloading) on|off

* ``offloading``: can be any of these offloading capability:
                  all, vlan_strip, ipv4_cksum, udp_cksum, tcp_cksum, tcp_lro,
                  qinq_strip, outer_ipv4_cksum, macsec_strip,
                  vlan_filter, vlan_extend, scatter, timestamp, security,
                  keep_crc

This command should be run when the port is stopped, or else it will fail.

config per port Tx offloading
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enable or disable port Tx offloading on all Tx queues of a port::

   testpmd> port config (port_id|all) tx_offload (offloading) on|off

* ``offloading``: can be any of these offloading capability:
                  all, vlan_insert, ipv4_cksum, udp_cksum, tcp_cksum,
                  sctp_cksum, tcp_tso, udp_tso, outer_ipv4_cksum,
                  qinq_insert, vxlan_tnl_tso, gre_tnl_tso,
                  ipip_tnl_tso, geneve_tnl_tso, macsec_insert,
                  mt_lockfree, multi_segs, mbuf_fast_free, security

This command should be run when the port is stopped, or else it will fail.

config per queue Tx offloading
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enable or disable a per queue Tx offloading only on a specific Tx queue::

   testpmd> port (port_id) txq (queue_id) tx_offload (offloading) on|off

* ``offloading``: can be any of these offloading capability:
                  all, vlan_insert, ipv4_cksum, udp_cksum, tcp_cksum,
                  sctp_cksum, tcp_tso, udp_tso, outer_ipv4_cksum,
                  qinq_insert, vxlan_tnl_tso, gre_tnl_tso,
                  ipip_tnl_tso, geneve_tnl_tso, macsec_insert,
                  mt_lockfree, multi_segs, mbuf_fast_free, security

This command should be run when the port is stopped, or else it will fail.

config per queue Tx affinity mapping
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Map a Tx queue with an aggregated port of the DPDK port (specified with port_id)::

   testpmd> port (port_id) txq (queue_id) affinity (value)

* ``affinity``: the number of the aggregated port.
                When multiple ports are aggregated into a single one,
                it allows to choose which port to use for Tx via a queue.

This command should be run when the port is stopped, otherwise it fails.


Config VXLAN Encap outer layers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure the outer layer to encapsulate a packet inside a VXLAN tunnel::

 set vxlan ip-version (ipv4|ipv6) vni (vni) udp-src (udp-src) \
 udp-dst (udp-dst) ip-src (ip-src) ip-dst (ip-dst) eth-src (eth-src) \
 eth-dst (eth-dst)

 set vxlan-with-vlan ip-version (ipv4|ipv6) vni (vni) udp-src (udp-src) \
 udp-dst (udp-dst) ip-src (ip-src) ip-dst (ip-dst) vlan-tci (vlan-tci) \
 eth-src (eth-src) eth-dst (eth-dst)

 set vxlan-tos-ttl ip-version (ipv4|ipv6) vni (vni) udp-src (udp-src) \
 udp-dst (udp-dst) ip-tos (ip-tos) ip-ttl (ip-ttl) ip-src (ip-src) \
 ip-dst (ip-dst) eth-src (eth-src) eth-dst (eth-dst)

These commands will set an internal configuration inside testpmd, any following
flow rule using the action vxlan_encap will use the last configuration set.
To have a different encapsulation header, one of those commands must be called
before the flow rule creation.

Config NVGRE Encap outer layers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure the outer layer to encapsulate a packet inside a NVGRE tunnel::

 set nvgre ip-version (ipv4|ipv6) tni (tni) ip-src (ip-src) ip-dst (ip-dst) \
        eth-src (eth-src) eth-dst (eth-dst)
 set nvgre-with-vlan ip-version (ipv4|ipv6) tni (tni) ip-src (ip-src) \
        ip-dst (ip-dst) vlan-tci (vlan-tci) eth-src (eth-src) eth-dst (eth-dst)

These commands will set an internal configuration inside testpmd, any following
flow rule using the action nvgre_encap will use the last configuration set.
To have a different encapsulation header, one of those commands must be called
before the flow rule creation.

Config L2 Encap
~~~~~~~~~~~~~~~

Configure the l2 to be used when encapsulating a packet with L2::

 set l2_encap ip-version (ipv4|ipv6) eth-src (eth-src) eth-dst (eth-dst)
 set l2_encap-with-vlan ip-version (ipv4|ipv6) vlan-tci (vlan-tci) \
        eth-src (eth-src) eth-dst (eth-dst)

Those commands will set an internal configuration inside testpmd, any following
flow rule using the action l2_encap will use the last configuration set.
To have a different encapsulation header, one of those commands must be called
before the flow rule creation.

Config L2 Decap
~~~~~~~~~~~~~~~

Configure the l2 to be removed when decapsulating a packet with L2::

 set l2_decap ip-version (ipv4|ipv6)
 set l2_decap-with-vlan ip-version (ipv4|ipv6)

Those commands will set an internal configuration inside testpmd, any following
flow rule using the action l2_decap will use the last configuration set.
To have a different encapsulation header, one of those commands must be called
before the flow rule creation.

Config MPLSoGRE Encap outer layers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure the outer layer to encapsulate a packet inside a MPLSoGRE tunnel::

 set mplsogre_encap ip-version (ipv4|ipv6) label (label) \
        ip-src (ip-src) ip-dst (ip-dst) eth-src (eth-src) eth-dst (eth-dst)
 set mplsogre_encap-with-vlan ip-version (ipv4|ipv6) label (label) \
        ip-src (ip-src) ip-dst (ip-dst) vlan-tci (vlan-tci) \
        eth-src (eth-src) eth-dst (eth-dst)

These commands will set an internal configuration inside testpmd, any following
flow rule using the action mplsogre_encap will use the last configuration set.
To have a different encapsulation header, one of those commands must be called
before the flow rule creation.

Config MPLSoGRE Decap outer layers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure the outer layer to decapsulate MPLSoGRE packet::

 set mplsogre_decap ip-version (ipv4|ipv6)
 set mplsogre_decap-with-vlan ip-version (ipv4|ipv6)

These commands will set an internal configuration inside testpmd, any following
flow rule using the action mplsogre_decap will use the last configuration set.
To have a different decapsulation header, one of those commands must be called
before the flow rule creation.

Config MPLSoUDP Encap outer layers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure the outer layer to encapsulate a packet inside a MPLSoUDP tunnel::

 set mplsoudp_encap ip-version (ipv4|ipv6) label (label) udp-src (udp-src) \
        udp-dst (udp-dst) ip-src (ip-src) ip-dst (ip-dst) \
        eth-src (eth-src) eth-dst (eth-dst)
 set mplsoudp_encap-with-vlan ip-version (ipv4|ipv6) label (label) \
        udp-src (udp-src) udp-dst (udp-dst) ip-src (ip-src) ip-dst (ip-dst) \
        vlan-tci (vlan-tci) eth-src (eth-src) eth-dst (eth-dst)

These commands will set an internal configuration inside testpmd, any following
flow rule using the action mplsoudp_encap will use the last configuration set.
To have a different encapsulation header, one of those commands must be called
before the flow rule creation.

Config MPLSoUDP Decap outer layers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure the outer layer to decapsulate MPLSoUDP packet::

 set mplsoudp_decap ip-version (ipv4|ipv6)
 set mplsoudp_decap-with-vlan ip-version (ipv4|ipv6)

These commands will set an internal configuration inside testpmd, any following
flow rule using the action mplsoudp_decap will use the last configuration set.
To have a different decapsulation header, one of those commands must be called
before the flow rule creation.

Config Raw Encapsulation
~~~~~~~~~~~~~~~~~~~~~~~~~

Configure the raw data to be used when encapsulating a packet by
rte_flow_action_raw_encap::

 set raw_encap {index} {item} [/ {item} [...]] / end_set

There are multiple global buffers for ``raw_encap``, this command will set one
internal buffer index by ``{index}``.
If there is no ``{index}`` specified::

 set raw_encap {item} [/ {item} [...]] / end_set

the default index ``0`` is used.
In order to use different encapsulating header, ``index`` must be specified
during the flow rule creation::

 testpmd> flow create 0 egress pattern eth / ipv4 / end actions
        raw_encap index 2 / end

Otherwise the default index ``0`` is used.

Config Raw Decapsulation
~~~~~~~~~~~~~~~~~~~~~~~~

Configure the raw data to be used when decapsulating a packet by
rte_flow_action_raw_decap::

 set raw_decap {index} {item} [/ {item} [...]] / end_set

There are multiple global buffers for ``raw_decap``, this command will set
one internal buffer index by ``{index}``.
If there is no ``{index}`` specified::

 set raw_decap {item} [/ {item} [...]] / end_set

the default index ``0`` is used.
In order to use different decapsulating header, ``index`` must be specified
during the flow rule creation::

 testpmd> flow create 0 egress pattern eth / ipv4 / end actions
          raw_encap index 3 / end

Otherwise the default index ``0`` is used.

Set fec mode
~~~~~~~~~~~~

Set fec mode for a specific port::

  testpmd> set port (port_id) fec_mode auto|off|rs|baser|llrs

Config Sample actions list
~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure the sample actions list to be used when sampling a packet by
rte_flow_action_sample::

 set sample_actions {index} {action} [/ {action} [...]] / end

There are multiple global buffers for ``sample_actions``, this command will set
one internal buffer index by ``{index}``.

In order to use different sample actions list, ``index`` must be specified
during the flow rule creation::

 testpmd> flow create 0 ingress pattern eth / ipv4 / end actions
        sample ratio 2 index 2 / end

Otherwise the default index ``0`` is used.

Port Functions
--------------

The following sections show functions for configuring ports.

.. note::

   Port configuration changes only become active when forwarding is started/restarted.

.. _port_attach:

port attach
~~~~~~~~~~~

Attach a port specified by pci address or virtual device args::

   testpmd> port attach (identifier)

To attach a new pci device, the device should be recognized by kernel first.
Then it should be moved under DPDK management.
Finally the port can be attached to testpmd.

For example, to move a pci device using ixgbe under DPDK management:

.. code-block:: console

   # Check the status of the available devices.
   ./usertools/dpdk-devbind.py --status

   Network devices using DPDK-compatible driver
   ============================================
   <none>

   Network devices using kernel driver
   ===================================
   0000:0a:00.0 '82599ES 10-Gigabit' if=eth2 drv=ixgbe unused=


   # Bind the device to igb_uio.
   sudo ./usertools/dpdk-devbind.py -b igb_uio 0000:0a:00.0


   # Recheck the status of the devices.
   ./usertools/dpdk-devbind.py --status
   Network devices using DPDK-compatible driver
   ============================================
   0000:0a:00.0 '82599ES 10-Gigabit' drv=igb_uio unused=

To attach a port created by virtual device, above steps are not needed.

For example, to attach a port whose pci address is 0000:0a:00.0.

.. code-block:: console

   testpmd> port attach 0000:0a:00.0
   Attaching a new port...
   EAL: PCI device 0000:0a:00.0 on NUMA socket -1
   EAL:   probe driver: 8086:10fb rte_ixgbe_pmd
   EAL:   PCI memory mapped at 0x7f83bfa00000
   EAL:   PCI memory mapped at 0x7f83bfa80000
   PMD: eth_ixgbe_dev_init(): MAC: 2, PHY: 18, SFP+: 5
   PMD: eth_ixgbe_dev_init(): port 0 vendorID=0x8086 deviceID=0x10fb
   Port 0 is attached. Now total ports is 1
   Done

For example, to attach a port created by pcap PMD.

.. code-block:: console

   testpmd> port attach net_pcap0
   Attaching a new port...
   PMD: Initializing pmd_pcap for net_pcap0
   PMD: Creating pcap-backed ethdev on numa socket 0
   Port 0 is attached. Now total ports is 1
   Done

In this case, identifier is ``net_pcap0``.
This identifier format is the same as ``--vdev`` format of DPDK applications.

For example, to re-attach a bonding port which has been previously detached,
the mode and slave parameters must be given.

.. code-block:: console

   testpmd> port attach net_bond_0,mode=0,slave=1
   Attaching a new port...
   EAL: Initializing pmd_bond for net_bond_0
   EAL: Create bonding device net_bond_0 on port 0 in mode 0 on socket 0.
   Port 0 is attached. Now total ports is 1
   Done


port detach
~~~~~~~~~~~

Detach a specific port::

   testpmd> port detach (port_id)

Before detaching a port, the port should be stopped and closed.

For example, to detach a pci device port 0.

.. code-block:: console

   testpmd> port stop 0
   Stopping ports...
   Done
   testpmd> port close 0
   Closing ports...
   Done

   testpmd> port detach 0
   Detaching a port...
   EAL: PCI device 0000:0a:00.0 on NUMA socket -1
   EAL:   remove driver: 8086:10fb rte_ixgbe_pmd
   EAL:   PCI memory unmapped at 0x7f83bfa00000
   EAL:   PCI memory unmapped at 0x7f83bfa80000
   Done


For example, to detach a virtual device port 0.

.. code-block:: console

   testpmd> port stop 0
   Stopping ports...
   Done
   testpmd> port close 0
   Closing ports...
   Done

   testpmd> port detach 0
   Detaching a port...
   PMD: Closing pcap ethdev on numa socket 0
   Port 'net_pcap0' is detached. Now total ports is 0
   Done

To remove a pci device completely from the system, first detach the port from testpmd.
Then the device should be moved under kernel management.
Finally the device can be removed using kernel pci hotplug functionality.

For example, to move a pci device under kernel management:

.. code-block:: console

   sudo ./usertools/dpdk-devbind.py -b ixgbe 0000:0a:00.0

   ./usertools/dpdk-devbind.py --status

   Network devices using DPDK-compatible driver
   ============================================
   <none>

   Network devices using kernel driver
   ===================================
   0000:0a:00.0 '82599ES 10-Gigabit' if=eth2 drv=ixgbe unused=igb_uio

To remove a port created by a virtual device, above steps are not needed.

port start
~~~~~~~~~~

Start all ports or a specific port::

   testpmd> port start (port_id|all)

port stop
~~~~~~~~~

Stop all ports or a specific port::

   testpmd> port stop (port_id|all)

port close
~~~~~~~~~~

Close all ports or a specific port::

   testpmd> port close (port_id|all)

port reset
~~~~~~~~~~

Reset all ports or a specific port::

   testpmd> port reset (port_id|all)

User should stop port(s) before resetting and (re-)start after reset.

port config - queue ring size
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure a rx/tx queue ring size::

   testpmd> port (port_id) (rxq|txq) (queue_id) ring_size (value)

Only take effect after command that (re-)start the port or command that setup specific queue.

port start/stop queue
~~~~~~~~~~~~~~~~~~~~~

Start/stop a rx/tx queue on a specific port::

   testpmd> port (port_id) (rxq|txq) (queue_id) (start|stop)

port config - queue deferred start
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Switch on/off deferred start of a specific port queue::

   testpmd> port (port_id) (rxq|txq) (queue_id) deferred_start (on|off)

port setup queue
~~~~~~~~~~~~~~~~~~~~~

Setup a rx/tx queue on a specific port::

   testpmd> port (port_id) (rxq|txq) (queue_id) setup

Only take effect when port is started.

port config - speed
~~~~~~~~~~~~~~~~~~~

Set the speed and duplex mode for all ports or a specific port::

   testpmd> port config (port_id|all) speed (10|100|1000|2500|5000|10000|25000|40000|50000|100000|200000|400000|auto) \
            duplex (half|full|auto)

port config - queues/descriptors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set number of queues/descriptors for rxq, txq, rxd and txd::

   testpmd> port config all (rxq|txq|rxd|txd) (value)

This is equivalent to the ``--rxq``, ``--txq``, ``--rxd`` and ``--txd`` command-line options.

port config - max-pkt-len
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the maximum packet length::

   testpmd> port config all max-pkt-len (value)

This is equivalent to the ``--max-pkt-len`` command-line option.

port config - max-lro-pkt-size
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the maximum LRO aggregated packet size::

   testpmd> port config all max-lro-pkt-size (value)

This is equivalent to the ``--max-lro-pkt-size`` command-line option.

port config - Drop Packets
~~~~~~~~~~~~~~~~~~~~~~~~~~

Enable or disable packet drop on all RX queues of all ports when no receive buffers available::

   testpmd> port config all drop-en (on|off)

Packet dropping when no receive buffers available is off by default.

The ``on`` option is equivalent to the ``--enable-drop-en`` command-line option.

port config - RSS
~~~~~~~~~~~~~~~~~

Set the RSS (Receive Side Scaling) mode on or off::
   testpmd> port config all rss (all|default|level-default|level-outer|level-inner| \
                                 ip|tcp|udp|sctp|tunnel|vlan|none| \
                                 ipv4|ipv4-frag|ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other| \
                                 ipv6|ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp| \
                                 ipv6-other|ipv6-ex|ipv6-tcp-ex|ipv6-udp-ex| \
                                 l2-payload|port|vxlan|geneve|nvgre|gtpu|eth|s-vlan|c-vlan| \
                                 esp|ah|l2tpv3|pfcp|pppoe|ecpri|mpls|ipv4-chksum|l4-chksum| \
                                 l2tpv2|l3-pre96|l3-pre64|l3-pre56|l3-pre48|l3-pre40|l3-pre32| \
                                 l2-dst-only|l2-src-only|l4-dst-only|l4-src-only|l3-dst-only|l3-src-only|<rsstype_id>)

RSS is on by default.

The ``all`` option is equivalent to eth|vlan|ip|tcp|udp|sctp|ether|l2tpv3|esp|ah|pfcp|l2tpv2.

The ``default`` option enables all supported RSS types reported by device info.

The ``none`` option is equivalent to the ``--disable-rss`` command-line option.

port config - RSS Reta
~~~~~~~~~~~~~~~~~~~~~~

Set the RSS (Receive Side Scaling) redirection table::

   testpmd> port config all rss reta (hash,queue)[,(hash,queue)]

port config - DCB
~~~~~~~~~~~~~~~~~

Set the DCB mode for an individual port::

   testpmd> port config (port_id) dcb vt (on|off) (traffic_class) pfc (on|off)

The traffic class should be 4 or 8.

port config - Burst
~~~~~~~~~~~~~~~~~~~

Set the number of packets per burst::

   testpmd> port config all burst (value)

This is equivalent to the ``--burst`` command-line option.

port config - Threshold
~~~~~~~~~~~~~~~~~~~~~~~

Set thresholds for TX/RX queues::

   testpmd> port config all (threshold) (value)

Where the threshold type can be:

* ``txpt:`` Set the prefetch threshold register of the TX rings, 0 <= value <= 255.

* ``txht:`` Set the host threshold register of the TX rings, 0 <= value <= 255.

* ``txwt:`` Set the write-back threshold register of the TX rings, 0 <= value <= 255.

* ``rxpt:`` Set the prefetch threshold register of the RX rings, 0 <= value <= 255.

* ``rxht:`` Set the host threshold register of the RX rings, 0 <= value <= 255.

* ``rxwt:`` Set the write-back threshold register of the RX rings, 0 <= value <= 255.

* ``txfreet:`` Set the transmit free threshold of the TX rings, 0 <= value <= txd.

* ``rxfreet:`` Set the transmit free threshold of the RX rings, 0 <= value <= rxd.

* ``txrst:`` Set the transmit RS bit threshold of TX rings, 0 <= value <= txd.

These threshold options are also available from the command-line.

port config pctype mapping
~~~~~~~~~~~~~~~~~~~~~~~~~~

Reset pctype mapping table::

   testpmd> port config (port_id) pctype mapping reset

Update hardware defined pctype to software defined flow type mapping table::

   testpmd> port config (port_id) pctype mapping update (pctype_id_0[,pctype_id_1]*) (flow_type_id)

where:

* ``pctype_id_x``: hardware pctype id as index of bit in bitmask value of the pctype mapping table.

* ``flow_type_id``: software flow type id as the index of the pctype mapping table.

port config input set
~~~~~~~~~~~~~~~~~~~~~

Config RSS/FDIR/FDIR flexible payload input set for some pctype::

   testpmd> port config (port_id) pctype (pctype_id) \
            (hash_inset|fdir_inset|fdir_flx_inset) \
	    (get|set|clear) field (field_idx)

Clear RSS/FDIR/FDIR flexible payload input set for some pctype::

   testpmd> port config (port_id) pctype (pctype_id) \
            (hash_inset|fdir_inset|fdir_flx_inset) clear all

where:

* ``pctype_id``: hardware packet classification types.
* ``field_idx``: hardware field index.

port config udp_tunnel_port
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add/remove UDP tunnel port for VXLAN/GENEVE tunneling protocols::

    testpmd> port config (port_id) udp_tunnel_port add|rm vxlan|geneve|vxlan-gpe|ecpri (udp_port)

port config tx_metadata
~~~~~~~~~~~~~~~~~~~~~~~

Set Tx metadata value per port.
testpmd will add this value to any Tx packet sent from this port::

   testpmd> port config (port_id) tx_metadata (value)

port config dynf
~~~~~~~~~~~~~~~~

Set/clear dynamic flag per port.
testpmd will register this flag in the mbuf (same registration
for both Tx and Rx). Then set/clear this flag for each Tx
packet sent from this port. The set bit only works for Tx packet::

   testpmd> port config (port_id) dynf (name) (set|clear)

port config mtu
~~~~~~~~~~~~~~~

To configure MTU(Maximum Transmission Unit) on devices using testpmd::

   testpmd> port config mtu (port_id) (value)

port config rss hash key
~~~~~~~~~~~~~~~~~~~~~~~~

To configure the RSS hash key used to compute the RSS
hash of input [IP] packets received on port::

   testpmd> port config <port_id> rss-hash-key (ipv4|ipv4-frag|\
                     ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other|\
                     ipv6|ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|\
                     ipv6-other|l2-payload|ipv6-ex|ipv6-tcp-ex|\
                     ipv6-udp-ex <string of hex digits \
                     (variable length, NIC dependent)>)

port cleanup txq mbufs
~~~~~~~~~~~~~~~~~~~~~~

To cleanup txq mbufs currently cached by driver::

   testpmd> port cleanup (port_id) txq (queue_id) (free_cnt)

If the value of ``free_cnt`` is 0, driver should free all cached mbufs.

Device Functions
----------------

The following sections show functions for device operations.

device detach
~~~~~~~~~~~~~

Detach a device specified by pci address or virtual device args::

   testpmd> device detach (identifier)

Before detaching a device associated with ports, the ports should be stopped and closed.

For example, to detach a pci device whose address is 0002:03:00.0.

.. code-block:: console

    testpmd> device detach 0002:03:00.0
    Removing a device...
    Port 1 is now closed
    EAL: Releasing pci mapped resource for 0002:03:00.0
    EAL: Calling pci_unmap_resource for 0002:03:00.0 at 0x218a050000
    EAL: Calling pci_unmap_resource for 0002:03:00.0 at 0x218c050000
    Device 0002:03:00.0 is detached
    Now total ports is 1

For example, to detach a port created by pcap PMD.

.. code-block:: console

    testpmd> device detach net_pcap0
    Removing a device...
    Port 0 is now closed
    Device net_pcap0 is detached
    Now total ports is 0
    Done

In this case, identifier is ``net_pcap0``.
This identifier format is the same as ``--vdev`` format of DPDK applications.

Link Bonding Functions
----------------------

The Link Bonding functions make it possible to dynamically create and
manage link bonding devices from within testpmd interactive prompt.

See :doc:`../prog_guide/link_bonding_poll_mode_drv_lib` for more information.

Traffic Metering and Policing
-----------------------------

The following section shows functions for configuring traffic metering and
policing on the ethernet device through the use of generic ethdev API.

show port traffic management capability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show traffic metering and policing capability of the port::

   testpmd> show port meter cap (port_id)

add port meter profile (srTCM rfc2967)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add meter profile (srTCM rfc2697) to the ethernet device::

   testpmd> add port meter profile srtcm_rfc2697 (port_id) (profile_id) \
   (cir) (cbs) (ebs) (packet_mode)

where:

* ``profile_id``: ID for the meter profile.
* ``cir``: Committed Information Rate (CIR) (bytes per second or packets per second).
* ``cbs``: Committed Burst Size (CBS) (bytes or packets).
* ``ebs``: Excess Burst Size (EBS) (bytes or packets).
* ``packet_mode``: Packets mode for meter profile.

add port meter profile (trTCM rfc2968)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add meter profile (srTCM rfc2698) to the ethernet device::

   testpmd> add port meter profile trtcm_rfc2698 (port_id) (profile_id) \
   (cir) (pir) (cbs) (pbs) (packet_mode)

where:

* ``profile_id``: ID for the meter profile.
* ``cir``: Committed information rate (bytes per second or packets per second).
* ``pir``: Peak information rate (bytes per second or packets per second).
* ``cbs``: Committed burst size (bytes or packets).
* ``pbs``: Peak burst size (bytes or packets).
* ``packet_mode``: Packets mode for meter profile.

add port meter profile (trTCM rfc4115)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add meter profile (trTCM rfc4115) to the ethernet device::

   testpmd> add port meter profile trtcm_rfc4115 (port_id) (profile_id) \
   (cir) (eir) (cbs) (ebs) (packet_mode)

where:

* ``profile_id``: ID for the meter profile.
* ``cir``: Committed information rate (bytes per second or packets per second).
* ``eir``: Excess information rate (bytes per second or packets per second).
* ``cbs``: Committed burst size (bytes or packets).
* ``ebs``: Excess burst size (bytes or packets).
* ``packet_mode``: Packets mode for meter profile.

delete port meter profile
~~~~~~~~~~~~~~~~~~~~~~~~~

Delete meter profile from the ethernet device::

   testpmd> del port meter profile (port_id) (profile_id)

create port policy
~~~~~~~~~~~~~~~~~~

Create new policy object for the ethernet device::

   testpmd> add port meter policy (port_id) (policy_id) g_actions \
   {action} y_actions {action} r_actions {action}

where:

* ``policy_id``: policy ID.
* ``action``: action lists for green/yellow/red colors.

delete port policy
~~~~~~~~~~~~~~~~~~

Delete policy object for the ethernet device::

   testpmd> del port meter policy (port_id) (policy_id)

where:

* ``policy_id``: policy ID.

create port meter
~~~~~~~~~~~~~~~~~

Create new meter object for the ethernet device::

   testpmd> create port meter (port_id) (mtr_id) (profile_id) \
   (policy_id) (meter_enable) (stats_mask) (shared) (default_input_color) \
   (use_pre_meter_color) [(dscp_tbl_entry0) (dscp_tbl_entry1)...\
   (dscp_tbl_entry63)] [(vlan_tbl_entry0) (vlan_tbl_entry1) ... \
   (vlan_tbl_entry15)]

where:

* ``mtr_id``: meter object ID.
* ``profile_id``: ID for the meter profile.
* ``policy_id``: ID for the policy.
* ``meter_enable``: When this parameter has a non-zero value, the meter object
  gets enabled at the time of creation, otherwise remains disabled.
* ``stats_mask``: Mask of statistics counter types to be enabled for the
  meter object.
* ``shared``:  When this parameter has a non-zero value, the meter object is
  shared by multiple flows. Otherwise, meter object is used by single flow.
* ``default_input_color``:  Default input color for incoming packets.
  If incoming packet misses DSCP or VLAN input color table then it will be used
  as input color.
* ``use_pre_meter_color``: When this parameter has a non-zero value, the
  input color for the current meter object is determined by the latest meter
  object in the same flow. Otherwise, the current meter object uses the
  *dscp_table* to determine the input color.
* ``dscp_tbl_entryx``: DSCP table entry x providing meter providing input
  color, 0 <= x <= 63.
* ``vlan_tbl_entryx``: VLAN table entry x providing meter input color,
  0 <= x <= 15.

enable port meter
~~~~~~~~~~~~~~~~~

Enable meter for the ethernet device::

   testpmd> enable port meter (port_id) (mtr_id)

disable port meter
~~~~~~~~~~~~~~~~~~

Disable meter for the ethernet device::

   testpmd> disable port meter (port_id) (mtr_id)

delete port meter
~~~~~~~~~~~~~~~~~

Delete meter for the ethernet device::

   testpmd> del port meter (port_id) (mtr_id)

Set port meter profile
~~~~~~~~~~~~~~~~~~~~~~

Set meter profile for the ethernet device::

   testpmd> set port meter profile (port_id) (mtr_id) (profile_id)

set port meter dscp table
~~~~~~~~~~~~~~~~~~~~~~~~~

Set meter dscp table for the ethernet device::

   testpmd> set port meter dscp table (port_id) (mtr_id) (proto) \
   [(dscp_tbl_entry0) (dscp_tbl_entry1)...(dscp_tbl_entry63)]

set port meter vlan table
~~~~~~~~~~~~~~~~~~~~~~~~~
Set meter VLAN table for the Ethernet device::

   testpmd> set port meter vlan table (port_id) (mtr_id) (proto) \
   [(vlan_tbl_entry0) (vlan_tbl_entry1)...(vlan_tbl_entry15)]

set port meter protocol
~~~~~~~~~~~~~~~~~~~~~~~
Set meter protocol and corresponding priority::

   testpmd> set port meter proto (port_id) (mtr_id) (proto) (prio)

get port meter protocol
~~~~~~~~~~~~~~~~~~~~~~~
Get meter protocol::

   testpmd> get port meter proto (port_id) (mtr_id)

get port meter protocol priority
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Get priority associated to meter protocol::

   testpmd> get port meter proto_prio (port_id) (mtr_id) (proto)

set port meter stats mask
~~~~~~~~~~~~~~~~~~~~~~~~~

Set meter stats mask for the ethernet device::

   testpmd> set port meter stats mask (port_id) (mtr_id) (stats_mask)

where:

* ``stats_mask``: Bit mask indicating statistics counter types to be enabled.

show port meter stats
~~~~~~~~~~~~~~~~~~~~~

Show meter stats of the ethernet device::

   testpmd> show port meter stats (port_id) (mtr_id) (clear)

where:

* ``clear``: Flag that indicates whether the statistics counters should
  be cleared (i.e. set to zero) immediately after they have been read or not.

Traffic Management
------------------

The following section shows functions for configuring traffic management on
the ethernet device through the use of generic TM API.

show port traffic management capability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show traffic management capability of the port::

   testpmd> show port tm cap (port_id)

show port traffic management capability (hierarchy level)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show traffic management hierarchy level capability of the port::

   testpmd> show port tm level cap (port_id) (level_id)

show port traffic management capability (hierarchy node level)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show the traffic management hierarchy node capability of the port::

   testpmd> show port tm node cap (port_id) (node_id)

show port traffic management hierarchy node type
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show the port traffic management hierarchy node type::

   testpmd> show port tm node type (port_id) (node_id)

show port traffic management hierarchy node stats
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show the port traffic management hierarchy node statistics::

   testpmd> show port tm node stats (port_id) (node_id) (clear)

where:

* ``clear``: When this parameter has a non-zero value, the statistics counters
  are cleared (i.e. set to zero) immediately after they have been read,
  otherwise the statistics counters are left untouched.

Add port traffic management private shaper profile
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add the port traffic management private shaper profile::

   testpmd> add port tm node shaper profile (port_id) (shaper_profile_id) \
   (cmit_tb_rate) (cmit_tb_size) (peak_tb_rate) (peak_tb_size) \
   (packet_length_adjust) (packet_mode)

where:

* ``shaper_profile id``: Shaper profile ID for the new profile.
* ``cmit_tb_rate``: Committed token bucket rate (bytes per second or packets per second).
* ``cmit_tb_size``: Committed token bucket size (bytes or packets).
* ``peak_tb_rate``: Peak token bucket rate (bytes per second or packets per second).
* ``peak_tb_size``: Peak token bucket size (bytes or packets).
* ``packet_length_adjust``: The value (bytes) to be added to the length of
  each packet for the purpose of shaping. This parameter value can be used to
  correct the packet length with the framing overhead bytes that are consumed
  on the wire.
* ``packet_mode``: Shaper configured in packet mode. This parameter value if
  zero, configures shaper in byte mode and if non-zero configures it in packet
  mode.

Delete port traffic management private shaper profile
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Delete the port traffic management private shaper::

   testpmd> del port tm node shaper profile (port_id) (shaper_profile_id)

where:

* ``shaper_profile id``: Shaper profile ID that needs to be deleted.

Add port traffic management shared shaper
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create the port traffic management shared shaper::

   testpmd> add port tm node shared shaper (port_id) (shared_shaper_id) \
   (shaper_profile_id)

where:

* ``shared_shaper_id``: Shared shaper ID to be created.
* ``shaper_profile id``: Shaper profile ID for shared shaper.

Set port traffic management shared shaper
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Update the port traffic management shared shaper::

   testpmd> set port tm node shared shaper (port_id) (shared_shaper_id) \
   (shaper_profile_id)

where:

* ``shared_shaper_id``: Shared shaper ID to be update.
* ``shaper_profile id``: Shaper profile ID for shared shaper.

Delete port traffic management shared shaper
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Delete the port traffic management shared shaper::

   testpmd> del port tm node shared shaper (port_id) (shared_shaper_id)

where:

* ``shared_shaper_id``: Shared shaper ID to be deleted.

Set port traffic management hierarchy node private shaper
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

set the port traffic management hierarchy node private shaper::

   testpmd> set port tm node shaper profile (port_id) (node_id) \
   (shaper_profile_id)

where:

* ``shaper_profile id``: Private shaper profile ID to be enabled on the
  hierarchy node.

Add port traffic management WRED profile
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a new WRED profile::

   testpmd> add port tm node wred profile (port_id) (wred_profile_id) \
   (color_g) (min_th_g) (max_th_g) (maxp_inv_g) (wq_log2_g) \
   (color_y) (min_th_y) (max_th_y) (maxp_inv_y) (wq_log2_y) \
   (color_r) (min_th_r) (max_th_r) (maxp_inv_r) (wq_log2_r)

where:

* ``wred_profile id``: Identifier for the newly create WRED profile
* ``color_g``: Packet color (green)
* ``min_th_g``: Minimum queue threshold for packet with green color
* ``max_th_g``: Minimum queue threshold for packet with green color
* ``maxp_inv_g``: Inverse of packet marking probability maximum value (maxp)
* ``wq_log2_g``: Negated log2 of queue weight (wq)
* ``color_y``: Packet color (yellow)
* ``min_th_y``: Minimum queue threshold for packet with yellow color
* ``max_th_y``: Minimum queue threshold for packet with yellow color
* ``maxp_inv_y``: Inverse of packet marking probability maximum value (maxp)
* ``wq_log2_y``: Negated log2 of queue weight (wq)
* ``color_r``: Packet color (red)
* ``min_th_r``: Minimum queue threshold for packet with yellow color
* ``max_th_r``: Minimum queue threshold for packet with yellow color
* ``maxp_inv_r``: Inverse of packet marking probability maximum value (maxp)
* ``wq_log2_r``: Negated log2 of queue weight (wq)

Delete port traffic management WRED profile
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Delete the WRED profile::

   testpmd> del port tm node wred profile (port_id) (wred_profile_id)

Add port traffic management hierarchy nonleaf node
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add nonleaf node to port traffic management hierarchy::

   testpmd> add port tm nonleaf node (port_id) (node_id) (parent_node_id) \
   (priority) (weight) (level_id) (shaper_profile_id) \
   (n_sp_priorities) (stats_mask) (n_shared_shapers) \
   [(shared_shaper_0) (shared_shaper_1) ...] \

where:

* ``parent_node_id``: Node ID of the parent.
* ``priority``: Node priority (highest node priority is zero). This is used by
  the SP algorithm running on the parent node for scheduling this node.
* ``weight``: Node weight (lowest weight is one). The node weight is relative
  to the weight sum of all siblings that have the same priority. It is used by
  the WFQ algorithm running on the parent node for scheduling this node.
* ``level_id``: Hierarchy level of the node.
* ``shaper_profile_id``: Shaper profile ID of the private shaper to be used by
  the node.
* ``n_sp_priorities``: Number of strict priorities.
* ``stats_mask``: Mask of statistics counter types to be enabled for this node.
* ``n_shared_shapers``: Number of shared shapers.
* ``shared_shaper_id``: Shared shaper id.

Add port traffic management hierarchy nonleaf node with packet mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add nonleaf node with packet mode to port traffic management hierarchy::

   testpmd> add port tm nonleaf node pktmode (port_id) (node_id) (parent_node_id) \
   (priority) (weight) (level_id) (shaper_profile_id) \
   (n_sp_priorities) (stats_mask) (n_shared_shapers) \
   [(shared_shaper_0) (shared_shaper_1) ...] \

where:

* ``parent_node_id``: Node ID of the parent.
* ``priority``: Node priority (highest node priority is zero). This is used by
  the SP algorithm running on the parent node for scheduling this node.
* ``weight``: Node weight (lowest weight is one). The node weight is relative
  to the weight sum of all siblings that have the same priority. It is used by
  the WFQ algorithm running on the parent node for scheduling this node.
* ``level_id``: Hierarchy level of the node.
* ``shaper_profile_id``: Shaper profile ID of the private shaper to be used by
  the node.
* ``n_sp_priorities``: Number of strict priorities. Packet mode is enabled on
  all of them.
* ``stats_mask``: Mask of statistics counter types to be enabled for this node.
* ``n_shared_shapers``: Number of shared shapers.
* ``shared_shaper_id``: Shared shaper id.

Add port traffic management hierarchy leaf node
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add leaf node to port traffic management hierarchy::

   testpmd> add port tm leaf node (port_id) (node_id) (parent_node_id) \
   (priority) (weight) (level_id) (shaper_profile_id) \
   (cman_mode) (wred_profile_id) (stats_mask) (n_shared_shapers) \
   [(shared_shaper_id) (shared_shaper_id) ...] \

where:

* ``parent_node_id``: Node ID of the parent.
* ``priority``: Node priority (highest node priority is zero). This is used by
  the SP algorithm running on the parent node for scheduling this node.
* ``weight``: Node weight (lowest weight is one). The node weight is relative
  to the weight sum of all siblings that have the same priority. It is used by
  the WFQ algorithm running on the parent node for scheduling this node.
* ``level_id``: Hierarchy level of the node.
* ``shaper_profile_id``: Shaper profile ID of the private shaper to be used by
  the node.
* ``cman_mode``: Congestion management mode to be enabled for this node.
* ``wred_profile_id``: WRED profile id to be enabled for this node.
* ``stats_mask``: Mask of statistics counter types to be enabled for this node.
* ``n_shared_shapers``: Number of shared shapers.
* ``shared_shaper_id``: Shared shaper id.

Delete port traffic management hierarchy node
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Delete node from port traffic management hierarchy::

   testpmd> del port tm node (port_id) (node_id)

Update port traffic management hierarchy parent node
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Update port traffic management hierarchy parent node::

   testpmd> set port tm node parent (port_id) (node_id) (parent_node_id) \
   (priority) (weight)

This function can only be called after the hierarchy commit invocation. Its
success depends on the port support for this operation, as advertised through
the port capability set. This function is valid for all nodes of the traffic
management hierarchy except root node.

Suspend port traffic management hierarchy node
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   testpmd> suspend port tm node (port_id) (node_id)

Resume port traffic management hierarchy node
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   testpmd> resume port tm node (port_id) (node_id)

Commit port traffic management hierarchy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Commit the traffic management hierarchy on the port::

   testpmd> port tm hierarchy commit (port_id) (clean_on_fail)

where:

* ``clean_on_fail``: When set to non-zero, hierarchy is cleared on function
  call failure. On the other hand, hierarchy is preserved when this parameter
  is equal to zero.

Set port traffic management mark VLAN dei
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enables/Disables the traffic management marking on the port for VLAN packets::

   testpmd> set port tm mark vlan_dei <port_id> <green> <yellow> <red>

where:

* ``port_id``: The port which on which VLAN packets marked as ``green`` or
  ``yellow`` or ``red`` will have dei bit enabled

* ``green`` enable 1, disable 0 marking for dei bit of VLAN packets marked as green

* ``yellow`` enable 1, disable 0 marking for dei bit of VLAN packets marked as yellow

* ``red`` enable 1, disable 0 marking for dei bit of VLAN packets marked as red

Set port traffic management mark IP dscp
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enables/Disables the traffic management marking on the port for IP dscp packets::

   testpmd> set port tm mark ip_dscp <port_id> <green> <yellow> <red>

where:

* ``port_id``: The port which on which IP packets marked as ``green`` or
  ``yellow`` or ``red`` will have IP dscp bits updated

* ``green`` enable 1, disable 0 marking IP dscp to low drop precedence for green packets

* ``yellow`` enable 1, disable 0 marking IP dscp to medium drop precedence for yellow packets

* ``red`` enable 1, disable 0 marking IP dscp to high drop precedence for red packets

Set port traffic management mark IP ecn
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enables/Disables the traffic management marking on the port for IP ecn packets::

   testpmd> set port tm mark ip_ecn <port_id> <green> <yellow> <red>

where:

* ``port_id``: The port which on which IP packets marked as ``green`` or
  ``yellow`` or ``red`` will have IP ecn bits updated

* ``green`` enable 1, disable 0 marking IP ecn for green marked packets with ecn of 2'b01  or 2'b10
  to ecn of 2'b11 when IP is caring TCP or SCTP

* ``yellow`` enable 1, disable 0 marking IP ecn for yellow marked packets with ecn of 2'b01  or 2'b10
  to ecn of 2'b11 when IP is caring TCP or SCTP

* ``red`` enable 1, disable 0 marking IP ecn for yellow marked packets with ecn of 2'b01  or 2'b10
  to ecn of 2'b11 when IP is caring TCP or SCTP

Congestion Management
---------------------

Get capabilities
~~~~~~~~~~~~~~~~

Retrieve congestion management capabilities supported by driver for given port.
Below example command retrieves capabilities for port 0::

   testpmd> show port cman capa 0

Get configuration
~~~~~~~~~~~~~~~~~

Retrieve congestion management configuration for given port.
Below example command retrieves configuration for port 0::

   testpmd> show port cman config 0

Set configuration
~~~~~~~~~~~~~~~~~

Configures congestion management settings on given queue
or mempool associated with queue.
Below example command configures RED as congestion management algorithm
for port 0 and queue 0::

   testpmd> set port cman config 0 0 obj queue mode red 10 100 1

Filter Functions
----------------

This section details the available filter functions that are available.

Note these functions interface the deprecated legacy filtering framework,
superseded by *rte_flow*. See `Flow rules management`_.

.. _testpmd_rte_flow:

Flow rules management
---------------------

Control of the generic flow API (*rte_flow*) is fully exposed through the
``flow`` command (configuration, validation, creation, destruction, queries
and operation modes).

Considering *rte_flow* overlaps with all `Filter Functions`_, using both
features simultaneously may cause undefined side-effects and is therefore
not recommended.

``flow`` syntax
~~~~~~~~~~~~~~~

Because the ``flow`` command uses dynamic tokens to handle the large number
of possible flow rules combinations, its behavior differs slightly from
other commands, in particular:

- Pressing *?* or the *<tab>* key displays contextual help for the current
  token, not that of the entire command.

- Optional and repeated parameters are supported (provided they are listed
  in the contextual help).

The first parameter stands for the operation mode. Possible operations and
their general syntax are described below. They are covered in detail in the
following sections.

- Get info about flow engine::

   flow info {port_id}

- Configure flow engine::

   flow configure {port_id}
       [queues_number {number}] [queues_size {size}]
       [counters_number {number}]
       [aging_counters_number {number}]
       [meters_number {number}] [flags {number}]

- Create a pattern template::

   flow pattern_template {port_id} create [pattern_template_id {id}]
       [relaxed {boolean}] [ingress] [egress] [transfer]
       template {item} [/ {item} [...]] / end

- Destroy a pattern template::

   flow pattern_template {port_id} destroy pattern_template {id} [...]

- Create an actions template::

   flow actions_template {port_id} create [actions_template_id {id}]
       [ingress] [egress] [transfer]
       template {action} [/ {action} [...]] / end
       mask {action} [/ {action} [...]] / end

- Destroy an actions template::

   flow actions_template {port_id} destroy actions_template {id} [...]

- Create a table::

   flow table {port_id} create
       [table_id {id}]
       [group {group_id}] [priority {level}] [ingress] [egress] [transfer]
       rules_number {number}
       pattern_template {pattern_template_id}
       actions_template {actions_template_id}

- Destroy a table::

   flow table {port_id} destroy table {id} [...]

- Check whether a flow rule can be created::

   flow validate {port_id}
       [group {group_id}] [priority {level}] [ingress] [egress] [transfer]
       pattern {item} [/ {item} [...]] / end
       actions {action} [/ {action} [...]] / end

- Enqueue creation of a flow rule::

   flow queue {port_id} create {queue_id}
       [postpone {boolean}] template_table {table_id}
       pattern_template {pattern_template_index}
       actions_template {actions_template_index}
       pattern {item} [/ {item} [...]] / end
       actions {action} [/ {action} [...]] / end

- Enqueue destruction of specific flow rules::

   flow queue {port_id} destroy {queue_id}
       [postpone {boolean}] rule {rule_id} [...]

- Push enqueued operations::

   flow push {port_id} queue {queue_id}

- Pull all operations results from a queue::

   flow pull {port_id} queue {queue_id}

- Create a flow rule::

   flow create {port_id}
       [group {group_id}] [priority {level}] [ingress] [egress]
       [transfer] [tunnel_set {tunnel_id}] [tunnel_match {tunnel_id}]
       [user_id {user_id}] pattern {item} [/ {item} [...]] / end
       actions {action} [/ {action} [...]] / end

- Destroy specific flow rules::

   flow destroy {port_id} rule {rule_id} [...] [user_id]

- Destroy all flow rules::

   flow flush {port_id}

- Query an existing flow rule::

   flow query {port_id} {rule_id} {action} [user_id]

- List existing flow rules sorted by priority, filtered by group
  identifiers::

   flow list {port_id} [group {group_id}] [...]

- Restrict ingress traffic to the defined flow rules::

   flow isolate {port_id} {boolean}

- Dump internal representation information of all flows in hardware::

   flow dump {port_id} all {output_file} [user_id]

  for one flow::

   flow dump {port_id} rule {rule_id} {output_file} [user_id]

- List and destroy aged flow rules::

   flow aged {port_id} [destroy]

- Enqueue list and destroy aged flow rules::

   flow queue {port_id} aged {queue_id} [destroy]

- Tunnel offload - create a tunnel stub::

   flow tunnel create {port_id} type {tunnel_type}

- Tunnel offload - destroy a tunnel stub::

   flow tunnel destroy {port_id} id {tunnel_id}

- Tunnel offload - list port tunnel stubs::

   flow tunnel list {port_id}

Retrieving info about flow management engine
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow info`` retrieves info on pre-configurable resources in the underlying
device to give a hint of possible values for flow engine configuration.

``rte_flow_info_get()``::

   flow info {port_id}

If successful, it will show::

   Flow engine resources on port #[...]:
   Number of queues: #[...]
   Size of queues: #[...]
   Number of counters: #[...]
   Number of aging objects: #[...]
   Number of meters: #[...]

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

Configuring flow management engine
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow configure`` pre-allocates all the needed resources in the underlying
device to be used later at the flow creation. Flow queues are allocated as well
for asynchronous flow creation/destruction operations. It is bound to
``rte_flow_configure()``::

   flow configure {port_id}
       [queues_number {number}] [queues_size {size}]
       [counters_number {number}]
       [aging_counters_number {number}]
       [host_port {number}]
       [meters_number {number}]
       [flags {number}]

If successful, it will show::

   Configure flows on port #[...]: number of queues #[...] with #[...] elements

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

Creating pattern templates
~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow pattern_template create`` creates the specified pattern template.
It is bound to ``rte_flow_pattern_template_create()``::

   flow pattern_template {port_id} create [pattern_template_id {id}]
       [relaxed {boolean}] [ingress] [egress] [transfer]
	   template {item} [/ {item} [...]] / end

If successful, it will show::

   Pattern template #[...] created

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

This command uses the same pattern items as ``flow create``,
their format is described in `Creating flow rules`_.

Destroying pattern templates
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow pattern_template destroy`` destroys one or more pattern templates
from their template ID (as returned by ``flow pattern_template create``),
this command calls ``rte_flow_pattern_template_destroy()`` as many
times as necessary::

   flow pattern_template {port_id} destroy pattern_template {id} [...]

If successful, it will show::

   Pattern template #[...] destroyed

It does not report anything for pattern template IDs that do not exist.
The usual error message is shown when a pattern template cannot be destroyed::

   Caught error type [...] ([...]): [...]

Creating actions templates
~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow actions_template create`` creates the specified actions template.
It is bound to ``rte_flow_actions_template_create()``::

   flow actions_template {port_id} create [actions_template_id {id}]
       [ingress] [egress] [transfer]
	   template {action} [/ {action} [...]] / end
       mask {action} [/ {action} [...]] / end

If successful, it will show::

   Actions template #[...] created

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

This command uses the same actions as ``flow create``,
their format is described in `Creating flow rules`_.

Destroying actions templates
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow actions_template destroy`` destroys one or more actions templates
from their template ID (as returned by ``flow actions_template create``),
this command calls ``rte_flow_actions_template_destroy()`` as many
times as necessary::

   flow actions_template {port_id} destroy actions_template {id} [...]

If successful, it will show::

   Actions template #[...] destroyed

It does not report anything for actions template IDs that do not exist.
The usual error message is shown when an actions template cannot be destroyed::

   Caught error type [...] ([...]): [...]

Creating template table
~~~~~~~~~~~~~~~~~~~~~~~

``flow template_table create`` creates the specified template table.
It is bound to ``rte_flow_template_table_create()``::

   flow template_table {port_id} create
       [table_id {id}] [group {group_id}]
       [priority {level}] [ingress] [egress]
       [transfer [vport_orig] [wire_orig]]
       rules_number {number}
       pattern_template {pattern_template_id}
       actions_template {actions_template_id}

If successful, it will show::

   Template table #[...] created

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

Destroying flow table
~~~~~~~~~~~~~~~~~~~~~

``flow template_table destroy`` destroys one or more template tables
from their table ID (as returned by ``flow template_table create``),
this command calls ``rte_flow_template_table_destroy()`` as many
times as necessary::

   flow template_table {port_id} destroy table {id} [...]

If successful, it will show::

   Template table #[...] destroyed

It does not report anything for table IDs that do not exist.
The usual error message is shown when a table cannot be destroyed::

   Caught error type [...] ([...]): [...]

Pushing enqueued operations
~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow push`` pushes all the outstanding enqueued operations
to the underlying device immediately.
It is bound to ``rte_flow_push()``::

   flow push {port_id} queue {queue_id}

If successful, it will show::

   Queue #[...] operations pushed

The usual error message is shown when operations cannot be pushed::

   Caught error type [...] ([...]): [...]

Pulling flow operations results
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow pull`` asks the underlying device about flow queue operations
results and return all the processed (successfully or not) operations.
It is bound to ``rte_flow_pull()``::

   flow pull {port_id} queue {queue_id}

If successful, it will show::

   Queue #[...] pulled #[...] operations (#[...] failed, #[...] succeeded)

The usual error message is shown when operations results cannot be pulled::

   Caught error type [...] ([...]): [...]

Calculating hash
~~~~~~~~~~~~~~~~

``flow hash`` calculates the hash for a given pattern.
It is bound to ``rte_flow_calc_table_hash()``::

   flow hash {port_id} template_table {table_id}
       pattern_template {pattern_template_index}
       actions_template {actions_template_index}
       pattern {item} [/ {item} [...]] / end

If successful, it will show the calculated hash result as seen below::

   Hash results 0x[...]

Otherwise, it will show an error message of the form::

   Caught error type [...] ([...]): [...]

This command uses the same pattern items as ``flow create``,
their format is described in `Creating flow rules`_.

Creating a tunnel stub for offload
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow tunnel create`` setup a tunnel stub for tunnel offload flow rules::

   flow tunnel create {port_id} type {tunnel_type}

If successful, it will return a tunnel stub ID usable with other commands::

   port [...]: flow tunnel #[...] type [...]

Tunnel stub ID is relative to a port.

Destroying tunnel offload stub
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow tunnel destroy`` destroy port tunnel stub::

   flow tunnel destroy {port_id} id {tunnel_id}

Listing tunnel offload stubs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow tunnel list`` list port tunnel offload stubs::

   flow tunnel list {port_id}

Validating flow rules
~~~~~~~~~~~~~~~~~~~~~

``flow validate`` reports whether a flow rule would be accepted by the
underlying device in its current state but stops short of creating it. It is
bound to ``rte_flow_validate()``::

   flow validate {port_id}
      [group {group_id}] [priority {level}] [ingress] [egress] [transfer]
      pattern {item} [/ {item} [...]] / end
      actions {action} [/ {action} [...]] / end

If successful, it will show::

   Flow rule validated

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

This command uses the same parameters as ``flow create``, their format is
described in `Creating flow rules`_.

Check whether redirecting any Ethernet packet received on port 0 to RX queue
index 6 is supported::

   testpmd> flow validate 0 ingress pattern eth / end
      actions queue index 6 / end
   Flow rule validated
   testpmd>

Port 0 does not support TCPv6 rules::

   testpmd> flow validate 0 ingress pattern eth / ipv6 / tcp / end
      actions drop / end
   Caught error type 9 (specific pattern item): Invalid argument
   testpmd>

Creating flow rules
~~~~~~~~~~~~~~~~~~~

``flow create`` validates and creates the specified flow rule. It is bound
to ``rte_flow_create()``::

   flow create {port_id}
      [group {group_id}] [priority {level}] [ingress] [egress] [transfer]
      [tunnel_set {tunnel_id}] [tunnel_match {tunnel_id}]
      [user_id {user_id}] pattern {item} [/ {item} [...]] / end
      actions {action} [/ {action} [...]] / end

If successful, it will return a flow rule ID usable with other commands::

   Flow rule #[...] created

Or if ``user_id`` is provided::

   Flow rule #[...] created, user-id [...]

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

Parameters describe in the following order:

- Attributes (*group*, *priority*, *ingress*, *egress*, *transfer* tokens).
- Tunnel offload specification (tunnel_set, tunnel_match)
- User identifier for the flow.
- A matching pattern, starting with the *pattern* token and terminated by an
  *end* pattern item.
- Actions, starting with the *actions* token and terminated by an *end*
  action.

These translate directly to *rte_flow* objects provided as-is to the
underlying functions.

The shortest valid definition only comprises mandatory tokens::

   testpmd> flow create 0 pattern end actions end

Note that PMDs may refuse rules that essentially do nothing such as this
one.

**All unspecified object values are automatically initialized to 0.**

Enqueueing creation of flow rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow queue create`` adds creation operation of a flow rule to a queue.
It is bound to ``rte_flow_async_create()``::

   flow queue {port_id} create {queue_id}
       [postpone {boolean}] template_table {table_id}
       pattern_template {pattern_template_index}
       actions_template {actions_template_index}
       pattern {item} [/ {item} [...]] / end
       actions {action} [/ {action} [...]] / end

If successful, it will return a flow rule ID usable with other commands::

   Flow rule #[...] creaion enqueued

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

This command uses the same pattern items and actions as ``flow create``,
their format is described in `Creating flow rules`_.

``flow queue pull`` must be called to retrieve the operation status.

Attributes
^^^^^^^^^^

These tokens affect flow rule attributes (``struct rte_flow_attr``) and are
specified before the ``pattern`` token.

- ``group {group id}``: priority group.
- ``priority {level}``: priority level within group.
- ``ingress``: rule applies to ingress traffic.
- ``egress``: rule applies to egress traffic.
- ``transfer``: apply rule directly to endpoints found in pattern.

Please note that use of ``transfer`` attribute requires that the flow and
its indirect components be managed via so-called ``transfer`` proxy port.
See `show flow transfer proxy port ID for the given port`_ for details.

Each instance of an attribute specified several times overrides the previous
value as shown below (group 4 is used)::

   testpmd> flow create 0 group 42 group 24 group 4 [...]

Note that once enabled, ``ingress`` and ``egress`` cannot be disabled.

While not specifying a direction is an error, some rules may allow both
simultaneously.

Most rules affect RX therefore contain the ``ingress`` token::

   testpmd> flow create 0 ingress pattern [...]

Tunnel offload
^^^^^^^^^^^^^^

Indicate tunnel offload rule type

- ``tunnel_set {tunnel_id}``: mark rule as tunnel offload decap_set type.
- ``tunnel_match {tunnel_id}``:  mark rule as tunnel offload match type.

Matching pattern
^^^^^^^^^^^^^^^^

A matching pattern starts after the ``pattern`` token. It is made of pattern
items and is terminated by a mandatory ``end`` item.

Items are named after their type (*RTE_FLOW_ITEM_TYPE_* from ``enum
rte_flow_item_type``).

The ``/`` token is used as a separator between pattern items as shown
below::

   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / end [...]

Note that protocol items like these must be stacked from lowest to highest
layer to make sense. For instance, the following rule is either invalid or
unlikely to match any packet::

   testpmd> flow create 0 ingress pattern eth / udp / ipv4 / end [...]

More information on these restrictions can be found in the *rte_flow*
documentation.

Several items support additional specification structures, for example
``ipv4`` allows specifying source and destination addresses as follows::

   testpmd> flow create 0 ingress pattern eth / ipv4 src is 10.1.1.1
      dst is 10.2.0.0 / end [...]

This rule matches all IPv4 traffic with the specified properties.

In this example, ``src`` and ``dst`` are field names of the underlying
``struct rte_flow_item_ipv4`` object. All item properties can be specified
in a similar fashion.

The ``is`` token means that the subsequent value must be matched exactly,
and assigns ``spec`` and ``mask`` fields in ``struct rte_flow_item``
accordingly. Possible assignment tokens are:

- ``is``: match value perfectly (with full bit-mask).
- ``spec``: match value according to configured bit-mask.
- ``last``: specify upper bound to establish a range.
- ``mask``: specify bit-mask with relevant bits set to one.
- ``prefix``: generate bit-mask with <prefix-length> most-significant bits set to one.

These yield identical results::

   ipv4 src is 10.1.1.1

::

   ipv4 src spec 10.1.1.1 src mask 255.255.255.255

::

   ipv4 src spec 10.1.1.1 src prefix 32

::

   ipv4 src is 10.1.1.1 src last 10.1.1.1 # range with a single value

::

   ipv4 src is 10.1.1.1 src last 0 # 0 disables range

Inclusive ranges can be defined with ``last``::

   ipv4 src is 10.1.1.1 src last 10.2.3.4 # 10.1.1.1 to 10.2.3.4

Note that ``mask`` affects both ``spec`` and ``last``::

   ipv4 src is 10.1.1.1 src last 10.2.3.4 src mask 255.255.0.0
      # matches 10.1.0.0 to 10.2.255.255

Properties can be modified multiple times::

   ipv4 src is 10.1.1.1 src is 10.1.2.3 src is 10.2.3.4 # matches 10.2.3.4

::

   ipv4 src is 10.1.1.1 src prefix 24 src prefix 16 # matches 10.1.0.0/16

Pattern items
^^^^^^^^^^^^^

This section lists supported pattern items and their attributes, if any.

- ``end``: end list of pattern items.

- ``void``: no-op pattern item.

- ``invert``: perform actions when pattern does not match.

- ``any``: match any protocol for the current layer.

  - ``num {unsigned}``: number of layers covered.

- ``port_id``: match traffic from/to a given DPDK port ID.

  - ``id {unsigned}``: DPDK port ID.

- ``mark``: match value set in previously matched flow rule using the mark action.

  - ``id {unsigned}``: arbitrary integer value.

- ``raw``: match an arbitrary byte string.

  - ``relative {boolean}``: look for pattern after the previous item.
  - ``search {boolean}``: search pattern from offset (see also limit).
  - ``offset {integer}``: absolute or relative offset for pattern.
  - ``limit {unsigned}``: search area limit for start of pattern.
  - ``pattern {string}``: byte string to look for.
  - ``pattern_hex {string}``: byte string (provided in hexadecimal) to look for.

- ``eth``: match Ethernet header.

  - ``dst {MAC-48}``: destination MAC.
  - ``src {MAC-48}``: source MAC.
  - ``type {unsigned}``: EtherType or TPID.

- ``vlan``: match 802.1Q/ad VLAN tag.

  - ``tci {unsigned}``: tag control information.
  - ``pcp {unsigned}``: priority code point.
  - ``dei {unsigned}``: drop eligible indicator.
  - ``vid {unsigned}``: VLAN identifier.
  - ``inner_type {unsigned}``: inner EtherType or TPID.

- ``ipv4``: match IPv4 header.

  - ``version_ihl {unsigned}``: IPv4 version and IP header length.
  - ``tos {unsigned}``: type of service.
  - ``ttl {unsigned}``: time to live.
  - ``proto {unsigned}``: next protocol ID.
  - ``src {ipv4 address}``: source address.
  - ``dst {ipv4 address}``: destination address.

- ``ipv6``: match IPv6 header.

  - ``tc {unsigned}``: traffic class.
  - ``flow {unsigned}``: flow label.
  - ``proto {unsigned}``: protocol (next header).
  - ``hop {unsigned}``: hop limit.
  - ``src {ipv6 address}``: source address.
  - ``dst {ipv6 address}``: destination address.

- ``icmp``: match ICMP header.

  - ``type {unsigned}``: ICMP packet type.
  - ``code {unsigned}``: ICMP packet code.

- ``udp``: match UDP header.

  - ``src {unsigned}``: UDP source port.
  - ``dst {unsigned}``: UDP destination port.

- ``tcp``: match TCP header.

  - ``src {unsigned}``: TCP source port.
  - ``dst {unsigned}``: TCP destination port.

- ``sctp``: match SCTP header.

  - ``src {unsigned}``: SCTP source port.
  - ``dst {unsigned}``: SCTP destination port.
  - ``tag {unsigned}``: validation tag.
  - ``cksum {unsigned}``: checksum.

- ``vxlan``: match VXLAN header.

  - ``vni {unsigned}``: VXLAN identifier.
  - ``last_rsvd {unsigned}``: VXLAN last reserved 8-bits.

- ``e_tag``: match IEEE 802.1BR E-Tag header.

  - ``grp_ecid_b {unsigned}``: GRP and E-CID base.

- ``nvgre``: match NVGRE header.

  - ``tni {unsigned}``: virtual subnet ID.

- ``mpls``: match MPLS header.

  - ``label {unsigned}``: MPLS label.

- ``gre``: match GRE header.

  - ``protocol {unsigned}``: protocol type.

- ``gre_key``: match GRE optional key field.

  - ``value {unsigned}``: key value.

- ``gre_option``: match GRE optional fields(checksum/key/sequence).

  - ``checksum {unsigned}``: checksum value.
  - ``key {unsigned}``: key value.
  - ``sequence {unsigned}``: sequence number value.

- ``fuzzy``: fuzzy pattern match, expect faster than default.

  - ``thresh {unsigned}``: accuracy threshold.

- ``gtp``, ``gtpc``, ``gtpu``: match GTPv1 header.

  - ``teid {unsigned}``: tunnel endpoint identifier.

- ``geneve``: match GENEVE header.

  - ``vni {unsigned}``: virtual network identifier.
  - ``protocol {unsigned}``: protocol type.

- ``geneve-opt``: match GENEVE header option.

  - ``class {unsigned}``: GENEVE option class.
  - ``type {unsigned}``: GENEVE option type.
  - ``length {unsigned}``: GENEVE option length in 32-bit words.
  - ``data {hex string}``: GENEVE option data, the length is defined by
    ``length`` field.

- ``vxlan-gpe``: match VXLAN-GPE header.

  - ``vni {unsigned}``: VXLAN-GPE identifier.

- ``arp_eth_ipv4``: match ARP header for Ethernet/IPv4.

  - ``sha {MAC-48}``: sender hardware address.
  - ``spa {ipv4 address}``: sender IPv4 address.
  - ``tha {MAC-48}``: target hardware address.
  - ``tpa {ipv4 address}``: target IPv4 address.

- ``ipv6_ext``: match presence of any IPv6 extension header.

  - ``next_hdr {unsigned}``: next header.

- ``icmp6``: match any ICMPv6 header.

  - ``type {unsigned}``: ICMPv6 type.
  - ``code {unsigned}``: ICMPv6 code.

- ``icmp6_echo_request``: match ICMPv6 echo request.

  - ``ident {unsigned}``: ICMPv6 echo request identifier.
  - ``seq {unsigned}``: ICMPv6 echo request sequence number.

- ``icmp6_echo_reply``: match ICMPv6 echo reply.

  - ``ident {unsigned}``: ICMPv6 echo reply identifier.
  - ``seq {unsigned}``: ICMPv6 echo reply sequence number.

- ``icmp6_nd_ns``: match ICMPv6 neighbor discovery solicitation.

  - ``target_addr {ipv6 address}``: target address.

- ``icmp6_nd_na``: match ICMPv6 neighbor discovery advertisement.

  - ``target_addr {ipv6 address}``: target address.

- ``icmp6_nd_opt``: match presence of any ICMPv6 neighbor discovery option.

  - ``type {unsigned}``: ND option type.

- ``icmp6_nd_opt_sla_eth``: match ICMPv6 neighbor discovery source Ethernet
  link-layer address option.

  - ``sla {MAC-48}``: source Ethernet LLA.

- ``icmp6_nd_opt_tla_eth``: match ICMPv6 neighbor discovery target Ethernet
  link-layer address option.

  - ``tla {MAC-48}``: target Ethernet LLA.

- ``meta``: match application specific metadata.

  - ``data {unsigned}``: metadata value.

- ``gtp_psc``: match GTP PDU extension header with type 0x85.

  - ``pdu_type {unsigned}``: PDU type.

  - ``qfi {unsigned}``: QoS flow identifier.

- ``pppoes``, ``pppoed``: match PPPoE header.

  - ``session_id {unsigned}``: session identifier.

- ``pppoe_proto_id``: match PPPoE session protocol identifier.

  - ``proto_id {unsigned}``: PPP protocol identifier.

- ``l2tpv3oip``: match L2TPv3 over IP header.

  - ``session_id {unsigned}``: L2TPv3 over IP session identifier.

- ``ah``: match AH header.

  - ``spi {unsigned}``: security parameters index.

- ``pfcp``: match PFCP header.

  - ``s_field {unsigned}``: S field.
  - ``seid {unsigned}``: session endpoint identifier.

- ``integrity``: match packet integrity.

   - ``level {unsigned}``: Packet encapsulation level the item should
     apply to. See rte_flow_action_rss for details.
   - ``value {unsigned}``: A bitmask that specify what packet elements
     must be matched for integrity.

- ``conntrack``: match conntrack state.

- ``port_representor``: match traffic entering the embedded switch from the given ethdev

  - ``port_id {unsigned}``: ethdev port ID

- ``represented_port``: match traffic entering the embedded switch from
  the entity represented by the given ethdev

  - ``ethdev_port_id {unsigned}``: ethdev port ID

- ``l2tpv2``: match L2TPv2 header.

  - ``length {unsigned}``: L2TPv2 option length.
  - ``tunnel_id {unsigned}``: L2TPv2 tunnel identifier.
  - ``session_id {unsigned}``: L2TPv2 session identifier.
  - ``ns {unsigned}``: L2TPv2 option ns.
  - ``nr {unsigned}``: L2TPv2 option nr.
  - ``offset_size {unsigned}``: L2TPv2 option offset.

- ``ppp``: match PPP header.

  - ``addr {unsigned}``: PPP address.
  - ``ctrl {unsigned}``: PPP control.
  - ``proto_id {unsigned}``: PPP protocol identifier.

- ``ib_bth``: match InfiniBand BTH(base transport header).

  - ``opcode {unsigned}``: Opcode.
  - ``pkey {unsigned}``: Partition key.
  - ``dst_qp {unsigned}``: Destination Queue Pair.
  - ``psn {unsigned}``: Packet Sequence Number.

- ``meter``: match meter color.

  - ``color {value}``: meter color value (green/yellow/red).

- ``aggr_affinity``: match aggregated port.

  - ``affinity {value}``: aggregated port (starts from 1).

- ``tx_queue``: match Tx queue of sent packet.

  - ``tx_queue {value}``: send queue value (starts from 0).

- ``send_to_kernel``: send packets to kernel.

- ``ptype``: match the packet type (L2/L3/L4 and tunnel information).

        - ``packet_type {unsigned}``: packet type.


Actions list
^^^^^^^^^^^^

A list of actions starts after the ``actions`` token in the same fashion as
`Matching pattern`_; actions are separated by ``/`` tokens and the list is
terminated by a mandatory ``end`` action.

Actions are named after their type (*RTE_FLOW_ACTION_TYPE_* from ``enum
rte_flow_action_type``).

Dropping all incoming UDPv4 packets can be expressed as follows::

   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / end
      actions drop / end

Several actions have configurable properties which must be specified when
there is no valid default value. For example, ``queue`` requires a target
queue index.

This rule redirects incoming UDPv4 traffic to queue index 6::

   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / end
      actions queue index 6 / end

While this one could be rejected by PMDs (unspecified queue index)::

   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / end
      actions queue / end

As defined by *rte_flow*, the list is not ordered, all actions of a given
rule are performed simultaneously. These are equivalent::

   queue index 6 / void / mark id 42 / end

::

   void / mark id 42 / queue index 6 / end

All actions in a list should have different types, otherwise only the last
action of a given type is taken into account::

   queue index 4 / queue index 5 / queue index 6 / end # will use queue 6

::

   drop / drop / drop / end # drop is performed only once

::

   mark id 42 / queue index 3 / mark id 24 / end # mark will be 24

Considering they are performed simultaneously, opposite and overlapping
actions can sometimes be combined when the end result is unambiguous::

   drop / queue index 6 / end # drop has no effect

::

   queue index 6 / rss queues 6 7 8 / end # queue has no effect

::

   drop / passthru / end # drop has no effect

Note that PMDs may still refuse such combinations.

Actions
^^^^^^^

This section lists supported actions and their attributes, if any.

- ``end``: end list of actions.

- ``void``: no-op action.

- ``passthru``: let subsequent rule process matched packets.

- ``jump``: redirect traffic to group on device.

  - ``group {unsigned}``: group to redirect to.

- ``mark``: attach 32 bit value to packets.

  - ``id {unsigned}``: 32 bit value to return with packets.

- ``flag``: flag packets.

- ``queue``: assign packets to a given queue index.

  - ``index {unsigned}``: queue index to use.

- ``drop``: drop packets (note: passthru has priority).

- ``count``: enable counters for this rule.

- ``rss``: spread packets among several queues.

  - ``func {hash function}``: RSS hash function to apply, allowed tokens are
    ``toeplitz``, ``simple_xor``, ``symmetric_toeplitz`` and ``default``.

  - ``level {unsigned}``: encapsulation level for ``types``.

  - ``types [{RSS hash type} [...]] end``: specific RSS hash types.
    Note that an empty list does not disable RSS but instead requests
    unspecified "best-effort" settings.

  - ``key {string}``: RSS hash key, overrides ``key_len``.

  - ``key_len {unsigned}``: RSS hash key length in bytes, can be used in
    conjunction with ``key`` to pad or truncate it.

  - ``queues [{unsigned} [...]] end``: queue indices to use.

- ``pf``: direct traffic to physical function.

- ``vf``: direct traffic to a virtual function ID.

  - ``original {boolean}``: use original VF ID if possible.
  - ``id {unsigned}``: VF ID.

- ``port_id``: direct matching traffic to a given DPDK port ID.

  - ``original {boolean}``: use original DPDK port ID if possible.
  - ``id {unsigned}``: DPDK port ID.

- ``of_set_mpls_ttl``: OpenFlow's ``OFPAT_SET_MPLS_TTL``.

  - ``mpls_ttl``: MPLS TTL.

- ``of_dec_mpls_ttl``: OpenFlow's ``OFPAT_DEC_MPLS_TTL``.

- ``of_set_nw_ttl``: OpenFlow's ``OFPAT_SET_NW_TTL``.

  - ``nw_ttl``: IP TTL.

- ``of_dec_nw_ttl``: OpenFlow's ``OFPAT_DEC_NW_TTL``.

- ``of_copy_ttl_out``: OpenFlow's ``OFPAT_COPY_TTL_OUT``.

- ``of_copy_ttl_in``: OpenFlow's ``OFPAT_COPY_TTL_IN``.

- ``of_pop_vlan``: OpenFlow's ``OFPAT_POP_VLAN``.

- ``of_push_vlan``: OpenFlow's ``OFPAT_PUSH_VLAN``.

  - ``ethertype``: Ethertype.

- ``of_set_vlan_vid``: OpenFlow's ``OFPAT_SET_VLAN_VID``.

  - ``vlan_vid``: VLAN id.

- ``of_set_vlan_pcp``: OpenFlow's ``OFPAT_SET_VLAN_PCP``.

  - ``vlan_pcp``: VLAN priority.

- ``of_pop_mpls``: OpenFlow's ``OFPAT_POP_MPLS``.

  - ``ethertype``: Ethertype.

- ``of_push_mpls``: OpenFlow's ``OFPAT_PUSH_MPLS``.

  - ``ethertype``: Ethertype.

- ``vxlan_encap``: Performs a VXLAN encapsulation, outer layer configuration
  is done through `Config VXLAN Encap outer layers`_.

- ``vxlan_decap``: Performs a decapsulation action by stripping all headers of
  the VXLAN tunnel network overlay from the matched flow.

- ``nvgre_encap``: Performs a NVGRE encapsulation, outer layer configuration
  is done through `Config NVGRE Encap outer layers`_.

- ``nvgre_decap``: Performs a decapsulation action by stripping all headers of
  the NVGRE tunnel network overlay from the matched flow.

- ``l2_encap``: Performs a L2 encapsulation, L2 configuration
  is done through `Config L2 Encap`_.

- ``l2_decap``: Performs a L2 decapsulation, L2 configuration
  is done through `Config L2 Decap`_.

- ``mplsogre_encap``: Performs a MPLSoGRE encapsulation, outer layer
  configuration is done through `Config MPLSoGRE Encap outer layers`_.

- ``mplsogre_decap``: Performs a MPLSoGRE decapsulation, outer layer
  configuration is done through `Config MPLSoGRE Decap outer layers`_.

- ``mplsoudp_encap``: Performs a MPLSoUDP encapsulation, outer layer
  configuration is done through `Config MPLSoUDP Encap outer layers`_.

- ``mplsoudp_decap``: Performs a MPLSoUDP decapsulation, outer layer
  configuration is done through `Config MPLSoUDP Decap outer layers`_.

- ``set_ipv4_src``: Set a new IPv4 source address in the outermost IPv4 header.

  - ``ipv4_addr``: New IPv4 source address.

- ``set_ipv4_dst``: Set a new IPv4 destination address in the outermost IPv4
  header.

  - ``ipv4_addr``: New IPv4 destination address.

- ``set_ipv6_src``: Set a new IPv6 source address in the outermost IPv6 header.

  - ``ipv6_addr``: New IPv6 source address.

- ``set_ipv6_dst``: Set a new IPv6 destination address in the outermost IPv6
  header.

  - ``ipv6_addr``: New IPv6 destination address.

- ``set_tp_src``: Set a new source port number in the outermost TCP/UDP
  header.

  - ``port``: New TCP/UDP source port number.

- ``set_tp_dst``: Set a new destination port number in the outermost TCP/UDP
  header.

  - ``port``: New TCP/UDP destination port number.

- ``mac_swap``: Swap the source and destination MAC addresses in the outermost
  Ethernet header.

- ``dec_ttl``: Performs a decrease TTL value action

- ``set_ttl``: Set TTL value with specified value
  - ``ttl_value {unsigned}``: The new TTL value to be set

- ``set_mac_src``: set source MAC address

  - ``mac_addr {MAC-48}``: new source MAC address

- ``set_mac_dst``: set destination MAC address

  - ``mac_addr {MAC-48}``: new destination MAC address

- ``inc_tcp_seq``: Increase sequence number in the outermost TCP header.

  - ``value {unsigned}``: Value to increase TCP sequence number by.

- ``dec_tcp_seq``: Decrease sequence number in the outermost TCP header.

  - ``value {unsigned}``: Value to decrease TCP sequence number by.

- ``inc_tcp_ack``: Increase acknowledgment number in the outermost TCP header.

  - ``value {unsigned}``: Value to increase TCP acknowledgment number by.

- ``dec_tcp_ack``: Decrease acknowledgment number in the outermost TCP header.

  - ``value {unsigned}``: Value to decrease TCP acknowledgment number by.

- ``set_ipv4_dscp``: Set IPv4 DSCP value with specified value

  - ``dscp_value {unsigned}``: The new DSCP value to be set

- ``set_ipv6_dscp``: Set IPv6 DSCP value with specified value

  - ``dscp_value {unsigned}``: The new DSCP value to be set

- ``indirect``: Use indirect action created via
  ``flow indirect_action {port_id} create``

  - ``indirect_action_id {unsigned}``: Indirect action ID to use

- ``color``: Color the packet to reflect the meter color result

  - ``type {value}``: Set color type with specified value(green/yellow/red)

- ``port_representor``: at embedded switch level, send matching traffic to
  the given ethdev

  - ``port_id {unsigned}``: ethdev port ID

- ``represented_port``: at embedded switch level, send matching traffic to
  the entity represented by the given ethdev

  - ``ethdev_port_id {unsigned}``: ethdev port ID

- ``meter_mark``:  meter the directed packets using profile and policy

  - ``mtr_profile {unsigned}``: meter profile ID to use
  - ``mtr_policy {unsigned}``: meter policy ID to use
  - ``mtr_color_mode {unsigned}``: meter color-awareness mode (blind/aware)
  - ``mtr_init_color {value}``: initial color value (green/yellow/red)
  - ``mtr_state {unsigned}``: meter state (disabled/enabled)

- ``modify_field``:  Modify packet field

  - ``op``: modify operation (set/add/sub)
  - ``dst_type``: the destination field to be modified, the supported fields as
    ``enum rte_flow_field_id`` listed.
  - ``dst_level``: destination field level.
  - ``dst_tag_index``: destination field tag array.
  - ``dst_type_id``: destination field type ID.
  - ``dst_class``: destination field class ID.
  - ``dst_offset``: destination field bit offset.
  - ``src_type``: the modify source field, the supported fields as
    ``enum rte_flow_field_id`` listed.
  - ``src_level``: source field level.
  - ``src_tag_index``: source field tag array.
  - ``src_type_id``: source field type ID.
  - ``src_class``: source field class ID.
  - ``src_offset``: source field bit offset.
  - ``src_value``: source immediate value.
  - ``src_ptr``: pointer to source immediate value.
  - ``width``: number of bits to copy.

Destroying flow rules
~~~~~~~~~~~~~~~~~~~~~

``flow destroy`` destroys one or more rules from their rule ID (as returned
by ``flow create``), this command calls ``rte_flow_destroy()`` as many
times as necessary::

   flow destroy {port_id} rule {rule_id} [...] [user_id]

If successful, it will show::

   Flow rule #[...] destroyed

Or if ``user_id`` flag is provided::

   Flow rule #[...] destroyed, user-id [...]

Optional ``user_id`` is a flag that signifies the rule ID
is the one provided by the user at creation.
It does not report anything for rule IDs that do not exist. The usual error
message is shown when a rule cannot be destroyed::

   Caught error type [...] ([...]): [...]

``flow flush`` destroys all rules on a device and does not take extra
arguments. It is bound to ``rte_flow_flush()``::

   flow flush {port_id}

Any errors are reported as above.

Creating several rules and destroying them::

   testpmd> flow create 0 ingress pattern eth / ipv6 / end
      actions queue index 2 / end
   Flow rule #0 created
   testpmd> flow create 0 ingress pattern eth / ipv4 / end
      actions queue index 3 / end
   Flow rule #1 created
   testpmd> flow destroy 0 rule 0 rule 1
   Flow rule #1 destroyed
   Flow rule #0 destroyed
   testpmd>

The same result can be achieved using ``flow flush``::

   testpmd> flow create 0 ingress pattern eth / ipv6 / end
      actions queue index 2 / end
   Flow rule #0 created
   testpmd> flow create 0 ingress pattern eth / ipv4 / end
      actions queue index 3 / end
   Flow rule #1 created
   testpmd> flow flush 0
   testpmd>

Non-existent rule IDs are ignored::

   testpmd> flow create 0 ingress pattern eth / ipv6 / end
      actions queue index 2 / end
   Flow rule #0 created
   testpmd> flow create 0 ingress pattern eth / ipv4 / end
      actions queue index 3 / end
   Flow rule #1 created
   testpmd> flow destroy 0 rule 42 rule 10 rule 2
   testpmd>
   testpmd> flow destroy 0 rule 0
   Flow rule #0 destroyed
   testpmd>

Enqueueing destruction of flow rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow queue destroy`` adds destruction operations to destroy one or more rules
from their rule ID (as returned by ``flow queue create``) to a queue,
this command calls ``rte_flow_async_destroy()`` as many times as necessary::

   flow queue {port_id} destroy {queue_id}
        [postpone {boolean}] rule {rule_id} [...]

If successful, it will show::

   Flow rule #[...] destruction enqueued

It does not report anything for rule IDs that do not exist. The usual error
message is shown when a rule cannot be destroyed::

   Caught error type [...] ([...]): [...]

``flow queue pull`` must be called to retrieve the operation status.

Querying flow rules
~~~~~~~~~~~~~~~~~~~

``flow query`` queries a specific action of a flow rule having that
ability. Such actions collect information that can be reported using this
command. It is bound to ``rte_flow_query()``::

   flow query {port_id} {rule_id} {action} [user_id]

Optional ``user_id`` is a flag that signifies the rule ID
is the one provided by the user at creation.
If successful, it will display either the retrieved data for known actions
or the following message::

   Cannot display result for action type [...] ([...])

Otherwise, it will complain either that the rule does not exist or that some
error occurred::

   Flow rule #[...] not found

::

   Caught error type [...] ([...]): [...]

Currently only the ``count`` action is supported. This action reports the
number of packets that hit the flow rule and the total number of bytes. Its
output has the following format::

   count:
    hits_set: [...] # whether "hits" contains a valid value
    bytes_set: [...] # whether "bytes" contains a valid value
    hits: [...] # number of packets
    bytes: [...] # number of bytes

Querying counters for TCPv6 packets redirected to queue 6::

   testpmd> flow create 0 ingress pattern eth / ipv6 / tcp / end
      actions queue index 6 / count / end
   Flow rule #4 created
   testpmd> flow query 0 4 count
   count:
    hits_set: 1
    bytes_set: 0
    hits: 386446
    bytes: 0
   testpmd>

Listing flow rules
~~~~~~~~~~~~~~~~~~

``flow list`` lists existing flow rules sorted by priority and optionally
filtered by group identifiers::

   flow list {port_id} [group {group_id}] [...]

This command only fails with the following message if the device does not
exist::

   Invalid port [...]

Output consists of a header line followed by a short description of each
flow rule, one per line. There is no output at all when no flow rules are
configured on the device::

   ID      Group   Prio    Attr    Rule
   [...]   [...]   [...]   [...]   [...]

``Attr`` column flags:

- ``i`` for ``ingress``.
- ``e`` for ``egress``.

Creating several flow rules and listing them::

   testpmd> flow create 0 ingress pattern eth / ipv4 / end
      actions queue index 6 / end
   Flow rule #0 created
   testpmd> flow create 0 ingress pattern eth / ipv6 / end
      actions queue index 2 / end
   Flow rule #1 created
   testpmd> flow create 0 priority 5 ingress pattern eth / ipv4 / udp / end
      actions rss queues 6 7 8 end / end
   Flow rule #2 created
   testpmd> flow list 0
   ID      Group   Prio    Attr    Rule
   0       0       0       i-      ETH IPV4 => QUEUE
   1       0       0       i-      ETH IPV6 => QUEUE
   2       0       5       i-      ETH IPV4 UDP => RSS
   testpmd>

Rules are sorted by priority (i.e. group ID first, then priority level)::

   testpmd> flow list 1
   ID      Group   Prio    Attr    Rule
   0       0       0       i-      ETH => COUNT
   6       0       500     i-      ETH IPV6 TCP => DROP COUNT
   5       0       1000    i-      ETH IPV6 ICMP => QUEUE
   1       24      0       i-      ETH IPV4 UDP => QUEUE
   4       24      10      i-      ETH IPV4 TCP => DROP
   3       24      20      i-      ETH IPV4 => DROP
   2       24      42      i-      ETH IPV4 UDP => QUEUE
   7       63      0       i-      ETH IPV6 UDP VXLAN => MARK QUEUE
   testpmd>

Output can be limited to specific groups::

   testpmd> flow list 1 group 0 group 63
   ID      Group   Prio    Attr    Rule
   0       0       0       i-      ETH => COUNT
   6       0       500     i-      ETH IPV6 TCP => DROP COUNT
   5       0       1000    i-      ETH IPV6 ICMP => QUEUE
   7       63      0       i-      ETH IPV6 UDP VXLAN => MARK QUEUE
   testpmd>

Toggling isolated mode
~~~~~~~~~~~~~~~~~~~~~~

``flow isolate`` can be used to tell the underlying PMD that ingress traffic
must only be injected from the defined flow rules; that no default traffic
is expected outside those rules and the driver is free to assign more
resources to handle them. It is bound to ``rte_flow_isolate()``::

 flow isolate {port_id} {boolean}

If successful, enabling or disabling isolated mode shows either::

 Ingress traffic on port [...]
    is now restricted to the defined flow rules

Or::

 Ingress traffic on port [...]
    is not restricted anymore to the defined flow rules

Otherwise, in case of error::

   Caught error type [...] ([...]): [...]

Mainly due to its side effects, PMDs supporting this mode may not have the
ability to toggle it more than once without reinitializing affected ports
first (e.g. by exiting testpmd).

Enabling isolated mode::

 testpmd> flow isolate 0 true
 Ingress traffic on port 0 is now restricted to the defined flow rules
 testpmd>

Disabling isolated mode::

 testpmd> flow isolate 0 false
 Ingress traffic on port 0 is not restricted anymore to the defined flow rules
 testpmd>

Dumping HW internal information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow dump`` dumps the hardware's internal representation information of
all flows. It is bound to ``rte_flow_dev_dump()``::

   flow dump {port_id} {output_file} [user_id]

If successful, it will show::

   Flow dump finished

Otherwise, it will complain error occurred::

   Caught error type [...] ([...]): [...]

Optional ``user_id`` is a flag that signifies the rule ID
is the one provided by the user at creation.

Listing and destroying aged flow rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow aged`` simply lists aged flow rules be get from api ``rte_flow_get_aged_flows``,
and ``destroy`` parameter can be used to destroy those flow rules in PMD::

   flow aged {port_id} [destroy]

Listing current aged flow rules::

   testpmd> flow aged 0
   Port 0 total aged flows: 0
   testpmd> flow create 0 ingress pattern eth / ipv4 src is 2.2.2.14 / end
      actions age timeout 5 / queue index 0 /  end
   Flow rule #0 created
   testpmd> flow create 0 ingress pattern eth / ipv4 src is 2.2.2.15 / end
      actions age timeout 4 / queue index 0 /  end
   Flow rule #1 created
   testpmd> flow create 0 ingress pattern eth / ipv4 src is 2.2.2.16 / end
      actions age timeout 2 / queue index 0 /  end
   Flow rule #2 created
   testpmd> flow create 0 ingress pattern eth / ipv4 src is 2.2.2.17 / end
      actions age timeout 3 / queue index 0 /  end
   Flow rule #3 created


Aged Rules are simply list as command ``flow list {port_id}``, but strip the detail rule
information, all the aged flows are sorted by the longest timeout time. For example, if
those rules be configured in the same time, ID 2 will be the first aged out rule, the next
will be ID 3, ID 1, ID 0::

   testpmd> flow aged 0
   Port 0 total aged flows: 4
   ID      Group   Prio    Attr
   2       0       0       i--
   3       0       0       i--
   1       0       0       i--
   0       0       0       i--

If attach ``destroy`` parameter, the command will destroy all the list aged flow rules::

   testpmd> flow aged 0 destroy
   Port 0 total aged flows: 4
   ID      Group   Prio    Attr
   2       0       0       i--
   3       0       0       i--
   1       0       0       i--
   0       0       0       i--

   Flow rule #2 destroyed
   Flow rule #3 destroyed
   Flow rule #1 destroyed
   Flow rule #0 destroyed
   4 flows be destroyed
   testpmd> flow aged 0
   Port 0 total aged flows: 0


Enqueueing listing and destroying aged flow rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow queue aged`` simply lists aged flow rules be get from
``rte_flow_get_q_aged_flows`` API, and ``destroy`` parameter can be used to
destroy those flow rules in PMD::

   flow queue {port_id} aged {queue_id} [destroy]

Listing current aged flow rules::

   testpmd> flow queue 0 aged 0
   Port 0 queue 0 total aged flows: 0
   testpmd> flow queue 0 create 0 ingress tanle 0 item_template 0 action_template 0
      pattern eth / ipv4 src is 2.2.2.14 / end
      actions age timeout 5 / queue index 0 /  end
   Flow rule #0 creation enqueued
   testpmd> flow queue 0 create 0 ingress tanle 0 item_template 0 action_template 0
      pattern eth / ipv4 src is 2.2.2.15 / end
      actions age timeout 4 / queue index 0 /  end
   Flow rule #1 creation enqueued
   testpmd> flow queue 0 create 0 ingress tanle 0 item_template 0 action_template 0
      pattern eth / ipv4 src is 2.2.2.16 / end
      actions age timeout 4 / queue index 0 /  end
   Flow rule #2 creation enqueued
   testpmd> flow queue 0 create 0 ingress tanle 0 item_template 0 action_template 0
      pattern eth / ipv4 src is 2.2.2.17 / end
      actions age timeout 4 / queue index 0 /  end
   Flow rule #3 creation enqueued
   testpmd> flow pull 0 queue 0
   Queue #0 pulled 4 operations (0 failed, 4 succeeded)

Aged Rules are simply list as command ``flow queue {port_id} list {queue_id}``,
but strip the detail rule information, all the aged flows are sorted by the
longest timeout time. For example, if those rules is configured in the same time,
ID 2 will be the first aged out rule, the next will be ID 3, ID 1, ID 0::

   testpmd> flow queue 0 aged 0
   Port 0 queue 0 total aged flows: 4
   ID      Group   Prio    Attr
   2       0       0       ---
   3       0       0       ---
   1       0       0       ---
   0       0       0       ---

   0 flows destroyed

If attach ``destroy`` parameter, the command will destroy all the list aged flow rules::

   testpmd> flow queue 0 aged 0 destroy
   Port 0 queue 0 total aged flows: 4
   ID      Group   Prio    Attr
   2       0       0       ---
   3       0       0       ---
   1       0       0       ---
   0       0       0       ---
   Flow rule #2 destruction enqueued
   Flow rule #3 destruction enqueued
   Flow rule #1 destruction enqueued
   Flow rule #0 destruction enqueued

   4 flows destroyed
   testpmd> flow queue 0 aged 0
   Port 0 total aged flows: 0

.. note::

   The queue must be empty before attaching ``destroy`` parameter.


Creating indirect actions
~~~~~~~~~~~~~~~~~~~~~~~~~

``flow indirect_action {port_id} create`` creates indirect action with optional
indirect action ID. It is bound to ``rte_flow_action_handle_create()``::

   flow indirect_action {port_id} create [action_id {indirect_action_id}]
      [ingress] [egress] [transfer] action {action} / end

If successful, it will show::

   Indirect action #[...] created

Otherwise, it will complain either that indirect action already exists or that
some error occurred::

   Indirect action #[...] is already assigned, delete it first

::

   Caught error type [...] ([...]): [...]

Create indirect rss action with id 100 to queues 1 and 2 on port 0::

   testpmd> flow indirect_action 0 create action_id 100 \
      ingress action rss queues 1 2 end / end

Create indirect rss action with id assigned by testpmd to queues 1 and 2 on
port 0::

	testpmd> flow indirect_action 0 create action_id \
		ingress action rss queues 0 1 end / end

Enqueueing creation of indirect actions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow queue indirect_action create`` adds creation operation of an indirect
action to a queue. It is bound to ``rte_flow_async_action_handle_create()``::

   flow queue {port_id} create {queue_id} [postpone {boolean}]
       table {table_id} item_template {item_template_id}
       action_template {action_template_id}
       pattern {item} [/ {item} [...]] / end
       actions {action} [/ {action} [...]] / end

If successful, it will show::

   Indirect action #[...] creation queued

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

This command uses the same parameters as  ``flow indirect_action create``,
described in `Creating indirect actions`_.

``flow queue pull`` must be called to retrieve the operation status.

Updating indirect actions
~~~~~~~~~~~~~~~~~~~~~~~~~

``flow indirect_action {port_id} update`` updates configuration of the indirect
action from its indirect action ID (as returned by
``flow indirect_action {port_id} create``). It is bound to
``rte_flow_action_handle_update()``::

   flow indirect_action {port_id} update {indirect_action_id}
      action {action} / end

If successful, it will show::

   Indirect action #[...] updated

Otherwise, it will complain either that indirect action not found or that some
error occurred::

   Failed to find indirect action #[...] on port [...]

::

   Caught error type [...] ([...]): [...]

Update indirect rss action having id 100 on port 0 with rss to queues 0 and 3
(in create example above rss queues were 1 and 2)::

   testpmd> flow indirect_action 0 update 100 action rss queues 0 3 end / end

Enqueueing update of indirect actions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow queue indirect_action update`` adds update operation for an indirect
action to a queue. It is bound to ``rte_flow_async_action_handle_update()``::

   flow queue {port_id} indirect_action {queue_id} update
      {indirect_action_id} [postpone {boolean}] action {action} / end

If successful, it will show::

   Indirect action #[...] update queued

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

``flow queue pull`` must be called to retrieve the operation status.

Destroying indirect actions
~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow indirect_action {port_id} destroy`` destroys one or more indirect actions
from their indirect action IDs (as returned by
``flow indirect_action {port_id} create``). It is bound to
``rte_flow_action_handle_destroy()``::

   flow indirect_action {port_id} destroy action_id {indirect_action_id} [...]

If successful, it will show::

   Indirect action #[...] destroyed

It does not report anything for indirect action IDs that do not exist.
The usual error message is shown when a indirect action cannot be destroyed::

   Caught error type [...] ([...]): [...]

Destroy indirect actions having id 100 & 101::

   testpmd> flow indirect_action 0 destroy action_id 100 action_id 101

Enqueueing destruction of indirect actions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow queue indirect_action destroy`` adds destruction operation to destroy
one or more indirect actions from their indirect action IDs (as returned by
``flow queue {port_id} indirect_action {queue_id} create``) to a queue.
It is bound to ``rte_flow_async_action_handle_destroy()``::

   flow queue {port_id} indirect_action {queue_id} destroy
      [postpone {boolean}] action_id {indirect_action_id} [...]

If successful, it will show::

   Indirect action #[...] destruction queued

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

``flow queue pull`` must be called to retrieve the operation status.

Query indirect actions
~~~~~~~~~~~~~~~~~~~~~~

``flow indirect_action {port_id} query`` queries the indirect action from its
indirect action ID (as returned by ``flow indirect_action {port_id} create``).
It is bound to ``rte_flow_action_handle_query()``::

  flow indirect_action {port_id} query {indirect_action_id}

Currently only rss indirect action supported. If successful, it will show::

   Indirect RSS action:
      refs:[...]

Otherwise, it will complain either that indirect action not found or that some
error occurred::

   Failed to find indirect action #[...] on port [...]

::

   Caught error type [...] ([...]): [...]

Query indirect action having id 100::

   testpmd> flow indirect_action 0 query 100

Enqueueing query of indirect actions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``flow queue indirect_action query`` adds query operation for an indirect
action to a queue. It is bound to ``rte_flow_async_action_handle_query()``::

   flow queue {port_id} indirect_action {queue_id} query
      {indirect_action_id} [postpone {boolean}]

If successful, it will show::

   Indirect action #[...] query queued

Otherwise it will show an error message of the form::

   Caught error type [...] ([...]): [...]

``flow queue pull`` must be called to retrieve the operation status.

Sample QinQ flow rules
~~~~~~~~~~~~~~~~~~~~~~

Before creating QinQ rule(s) the following commands should be issued to enable QinQ::

   testpmd> port stop 0
   testpmd> vlan set extend on 0

The above command sets the inner and outer TPID's to 0x8100.

To change the TPID's the following commands should be used::

   testpmd> vlan set outer tpid 0x88A8 0
   testpmd> vlan set inner tpid 0x8100 0
   testpmd> port start 0

Validate and create a QinQ rule on port 0 to steer traffic to a VF queue in a VM.

::

   testpmd> flow validate 0 ingress pattern eth / vlan tci is 123 /
       vlan tci is 456 / end actions vf id 1 / queue index 0 / end
   Flow rule #0 validated

   testpmd> flow create 0 ingress pattern eth / vlan tci is 4 /
       vlan tci is 456 / end actions vf id 123 / queue index 0 / end
   Flow rule #0 created

   testpmd> flow list 0
   ID      Group   Prio    Attr    Rule
   0       0       0       i-      ETH VLAN VLAN=>VF QUEUE

Validate and create a QinQ rule on port 0 to steer traffic to a queue on the host.

::

   testpmd> flow validate 0 ingress pattern eth / vlan tci is 321 /
        vlan tci is 654 / end actions pf / queue index 0 / end
   Flow rule #1 validated

   testpmd> flow create 0 ingress pattern eth / vlan tci is 321 /
        vlan tci is 654 / end actions pf / queue index 1 / end
   Flow rule #1 created

   testpmd> flow list 0
   ID      Group   Prio    Attr    Rule
   0       0       0       i-      ETH VLAN VLAN=>VF QUEUE
   1       0       0       i-      ETH VLAN VLAN=>PF QUEUE

Sample VXLAN flow rules
~~~~~~~~~~~~~~~~~~~~~~~

Before creating VXLAN rule(s), the UDP port should be added for VXLAN packet
filter on a port::

  testpmd> rx_vxlan_port add 4789 0

Create VXLAN rules on port 0 to steer traffic to PF queues.

::

  testpmd> flow create 0 ingress pattern eth / ipv4 / udp / vxlan /
         eth dst is 00:11:22:33:44:55 / end actions pf / queue index 1 / end
  Flow rule #0 created

  testpmd> flow create 0 ingress pattern eth / ipv4 / udp / vxlan vni is 3 /
         eth dst is 00:11:22:33:44:55 / end actions pf / queue index 2 / end
  Flow rule #1 created

  testpmd> flow create 0 ingress pattern eth / ipv4 / udp / vxlan /
         eth dst is 00:11:22:33:44:55 / vlan tci is 10 / end actions pf /
         queue index 3 / end
  Flow rule #2 created

  testpmd> flow create 0 ingress pattern eth / ipv4 / udp / vxlan vni is 5 /
         eth dst is 00:11:22:33:44:55 / vlan tci is 20 / end actions pf /
         queue index 4 / end
  Flow rule #3 created

  testpmd> flow create 0 ingress pattern eth dst is 00:00:00:00:01:00 / ipv4 /
         udp / vxlan vni is 6 /  eth dst is 00:11:22:33:44:55 / end actions pf /
         queue index 5 / end
  Flow rule #4 created

  testpmd> flow list 0
  ID      Group   Prio    Attr    Rule
  0       0       0       i-      ETH IPV4 UDP VXLAN ETH => QUEUE
  1       0       0       i-      ETH IPV4 UDP VXLAN ETH => QUEUE
  2       0       0       i-      ETH IPV4 UDP VXLAN ETH VLAN => QUEUE
  3       0       0       i-      ETH IPV4 UDP VXLAN ETH VLAN => QUEUE
  4       0       0       i-      ETH IPV4 UDP VXLAN ETH => QUEUE

Sample VXLAN encapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VXLAN encapsulation outer layer has default value pre-configured in testpmd
source code, those can be changed by using the following commands

IPv4 VXLAN outer header::

 testpmd> set vxlan ip-version ipv4 vni 4 udp-src 4 udp-dst 4 ip-src 127.0.0.1
        ip-dst 128.0.0.1 eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions vxlan_encap /
        queue index 0 / end

 testpmd> set vxlan-with-vlan ip-version ipv4 vni 4 udp-src 4 udp-dst 4 ip-src
         127.0.0.1 ip-dst 128.0.0.1 vlan-tci 34 eth-src 11:11:11:11:11:11
         eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions vxlan_encap /
         queue index 0 / end

 testpmd> set vxlan-tos-ttl ip-version ipv4 vni 4 udp-src 4 udp-dst 4 ip-tos 0
         ip-ttl 255 ip-src 127.0.0.1 ip-dst 128.0.0.1 eth-src 11:11:11:11:11:11
         eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions vxlan_encap /
         queue index 0 / end

IPv6 VXLAN outer header::

 testpmd> set vxlan ip-version ipv6 vni 4 udp-src 4 udp-dst 4 ip-src ::1
        ip-dst ::2222 eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions vxlan_encap /
         queue index 0 / end

 testpmd> set vxlan-with-vlan ip-version ipv6 vni 4 udp-src 4 udp-dst 4
         ip-src ::1 ip-dst ::2222 vlan-tci 34 eth-src 11:11:11:11:11:11
         eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions vxlan_encap /
         queue index 0 / end

 testpmd> set vxlan-tos-ttl ip-version ipv6 vni 4 udp-src 4 udp-dst 4
         ip-tos 0 ip-ttl 255 ::1 ip-dst ::2222 eth-src 11:11:11:11:11:11
         eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions vxlan_encap /
         queue index 0 / end

Sample NVGRE encapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

NVGRE encapsulation outer layer has default value pre-configured in testpmd
source code, those can be changed by using the following commands

IPv4 NVGRE outer header::

 testpmd> set nvgre ip-version ipv4 tni 4 ip-src 127.0.0.1 ip-dst 128.0.0.1
        eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions nvgre_encap /
        queue index 0 / end

 testpmd> set nvgre-with-vlan ip-version ipv4 tni 4 ip-src 127.0.0.1
         ip-dst 128.0.0.1 vlan-tci 34 eth-src 11:11:11:11:11:11
         eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions nvgre_encap /
         queue index 0 / end

IPv6 NVGRE outer header::

 testpmd> set nvgre ip-version ipv6 tni 4 ip-src ::1 ip-dst ::2222
        eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions nvgre_encap /
        queue index 0 / end

 testpmd> set nvgre-with-vlan ip-version ipv6 tni 4 ip-src ::1 ip-dst ::2222
        vlan-tci 34 eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern end actions nvgre_encap /
        queue index 0 / end

Sample L2 encapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

L2 encapsulation has default value pre-configured in testpmd
source code, those can be changed by using the following commands

L2 header::

 testpmd> set l2_encap ip-version ipv4
        eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern eth / ipv4 / udp / mpls / end actions
        mplsoudp_decap / l2_encap / end

L2 with VXLAN header::

 testpmd> set l2_encap-with-vlan ip-version ipv4 vlan-tci 34
         eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 ingress pattern eth / ipv4 / udp / mpls / end actions
        mplsoudp_decap / l2_encap / end

Sample L2 decapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

L2 decapsulation has default value pre-configured in testpmd
source code, those can be changed by using the following commands

L2 header::

 testpmd> set l2_decap
 testpmd> flow create 0 egress pattern eth / end actions l2_decap / mplsoudp_encap /
        queue index 0 / end

L2 with VXLAN header::

 testpmd> set l2_encap-with-vlan
 testpmd> flow create 0 egress pattern eth / end actions l2_encap / mplsoudp_encap /
         queue index 0 / end

Sample MPLSoGRE encapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MPLSoGRE encapsulation outer layer has default value pre-configured in testpmd
source code, those can be changed by using the following commands

IPv4 MPLSoGRE outer header::

 testpmd> set mplsogre_encap ip-version ipv4 label 4
        ip-src 127.0.0.1 ip-dst 128.0.0.1 eth-src 11:11:11:11:11:11
        eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 egress pattern eth / end actions l2_decap /
        mplsogre_encap / end

IPv4 MPLSoGRE with VLAN outer header::

 testpmd> set mplsogre_encap-with-vlan ip-version ipv4 label 4
        ip-src 127.0.0.1 ip-dst 128.0.0.1 vlan-tci 34
        eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 egress pattern eth / end actions l2_decap /
        mplsogre_encap / end

IPv6 MPLSoGRE outer header::

 testpmd> set mplsogre_encap ip-version ipv6 mask 4
        ip-src ::1 ip-dst ::2222 eth-src 11:11:11:11:11:11
        eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 egress pattern eth / end actions l2_decap /
        mplsogre_encap / end

IPv6 MPLSoGRE with VLAN outer header::

 testpmd> set mplsogre_encap-with-vlan ip-version ipv6 mask 4
        ip-src ::1 ip-dst ::2222 vlan-tci 34
        eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 egress pattern eth / end actions l2_decap /
        mplsogre_encap / end

Sample MPLSoGRE decapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MPLSoGRE decapsulation outer layer has default value pre-configured in testpmd
source code, those can be changed by using the following commands

IPv4 MPLSoGRE outer header::

 testpmd> set mplsogre_decap ip-version ipv4
 testpmd> flow create 0 ingress pattern eth / ipv4 / gre / mpls / end actions
        mplsogre_decap / l2_encap / end

IPv4 MPLSoGRE with VLAN outer header::

 testpmd> set mplsogre_decap-with-vlan ip-version ipv4
 testpmd> flow create 0 ingress pattern eth / vlan / ipv4 / gre / mpls / end
        actions mplsogre_decap / l2_encap / end

IPv6 MPLSoGRE outer header::

 testpmd> set mplsogre_decap ip-version ipv6
 testpmd> flow create 0 ingress pattern eth / ipv6 / gre / mpls / end
        actions mplsogre_decap / l2_encap / end

IPv6 MPLSoGRE with VLAN outer header::

 testpmd> set mplsogre_decap-with-vlan ip-version ipv6
 testpmd> flow create 0 ingress pattern eth / vlan / ipv6 / gre / mpls / end
        actions mplsogre_decap / l2_encap / end

Sample MPLSoUDP encapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MPLSoUDP encapsulation outer layer has default value pre-configured in testpmd
source code, those can be changed by using the following commands

IPv4 MPLSoUDP outer header::

 testpmd> set mplsoudp_encap ip-version ipv4 label 4 udp-src 5 udp-dst 10
        ip-src 127.0.0.1 ip-dst 128.0.0.1 eth-src 11:11:11:11:11:11
        eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 egress pattern eth / end actions l2_decap /
        mplsoudp_encap / end

IPv4 MPLSoUDP with VLAN outer header::

 testpmd> set mplsoudp_encap-with-vlan ip-version ipv4 label 4 udp-src 5
        udp-dst 10 ip-src 127.0.0.1 ip-dst 128.0.0.1 vlan-tci 34
        eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 egress pattern eth / end actions l2_decap /
        mplsoudp_encap / end

IPv6 MPLSoUDP outer header::

 testpmd> set mplsoudp_encap ip-version ipv6 mask 4 udp-src 5 udp-dst 10
        ip-src ::1 ip-dst ::2222 eth-src 11:11:11:11:11:11
        eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 egress pattern eth / end actions l2_decap /
        mplsoudp_encap / end

IPv6 MPLSoUDP with VLAN outer header::

 testpmd> set mplsoudp_encap-with-vlan ip-version ipv6 mask 4 udp-src 5
        udp-dst 10 ip-src ::1 ip-dst ::2222 vlan-tci 34
        eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
 testpmd> flow create 0 egress pattern eth / end actions l2_decap /
        mplsoudp_encap / end

Sample MPLSoUDP decapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MPLSoUDP decapsulation outer layer has default value pre-configured in testpmd
source code, those can be changed by using the following commands

IPv4 MPLSoUDP outer header::

 testpmd> set mplsoudp_decap ip-version ipv4
 testpmd> flow create 0 ingress pattern eth / ipv4 / udp / mpls / end actions
        mplsoudp_decap / l2_encap / end

IPv4 MPLSoUDP with VLAN outer header::

 testpmd> set mplsoudp_decap-with-vlan ip-version ipv4
 testpmd> flow create 0 ingress pattern eth / vlan / ipv4 / udp / mpls / end
        actions mplsoudp_decap / l2_encap / end

IPv6 MPLSoUDP outer header::

 testpmd> set mplsoudp_decap ip-version ipv6
 testpmd> flow create 0 ingress pattern eth / ipv6 / udp / mpls / end
        actions mplsoudp_decap / l2_encap / end

IPv6 MPLSoUDP with VLAN outer header::

 testpmd> set mplsoudp_decap-with-vlan ip-version ipv6
 testpmd> flow create 0 ingress pattern eth / vlan / ipv6 / udp / mpls / end
        actions mplsoudp_decap / l2_encap / end

Sample Raw encapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Raw encapsulation configuration can be set by the following commands

Encapsulating VxLAN::

 testpmd> set raw_encap 4 eth src is 10:11:22:33:44:55 / vlan tci is 1
        inner_type is 0x0800 / ipv4 / udp dst is 4789 / vxlan vni
        is 2 / end_set
 testpmd> flow create 0 egress pattern eth / ipv4 / end actions
        raw_encap index 4 / end

Sample Raw decapsulation rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Raw decapsulation configuration can be set by the following commands

Decapsulating VxLAN::

 testpmd> set raw_decap eth / ipv4 / udp / vxlan / end_set
 testpmd> flow create 0 ingress pattern eth / ipv4 / udp / vxlan / eth / ipv4 /
        end actions raw_decap / queue index 0 / end

Sample ESP rules
~~~~~~~~~~~~~~~~

ESP rules can be created by the following commands::

 testpmd> flow create 0 ingress pattern eth / ipv4 / esp spi is 1 / end actions
        queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv4 / udp / esp spi is 1 / end
        actions queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv6 / esp spi is 1 / end actions
        queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv6 / udp / esp spi is 1 / end
        actions queue index 3 / end

Sample AH rules
~~~~~~~~~~~~~~~~

AH rules can be created by the following commands::

 testpmd> flow create 0 ingress pattern eth / ipv4 / ah spi is 1 / end actions
        queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv4 / udp / ah spi is 1 / end
        actions queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv6 / ah spi is 1 / end actions
        queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv6 / udp / ah spi is 1 / end
        actions queue index 3 / end

Sample PFCP rules
~~~~~~~~~~~~~~~~~

PFCP rules can be created by the following commands(s_field need to be 1
if seid is set)::

 testpmd> flow create 0 ingress pattern eth / ipv4 / pfcp s_field is 0 / end
        actions queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv4 / pfcp s_field is 1
        seid is 1 / end actions queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv6 / pfcp s_field is 0 / end
        actions queue index 3 / end
 testpmd> flow create 0 ingress pattern eth / ipv6 / pfcp s_field is 1
        seid is 1 / end actions queue index 3 / end

Sample Sampling/Mirroring rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sample/Mirroring rules can be set by the following commands

NIC-RX Sampling rule, the matched ingress packets and sent to the queue 1,
and 50% packets are duplicated and marked with 0x1234 and sent to queue 0.

::

 testpmd> set sample_actions 0 mark id  0x1234 / queue index 0 / end
 testpmd> flow create 0 ingress group 1 pattern eth / end actions
        sample ratio 2 index 0 / queue index 1 / end

Match packets coming from a VM which is referred to by means of
its representor ethdev (port 1), mirror 50% of them to the
said representor (for bookkeeping) as well as encapsulate
all the packets and steer them to the physical port:

::

   testpmd> set sample_actions 0 port_representor ethdev_port_id 1 / end

   testpmd> set vxlan ip-version ipv4 vni 4 udp-src 32 udp-dst 4789 ip-src 127.0.0.1
      ip-dst 127.0.0.2 eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22

   testpmd> flow create 0 transfer pattern represented_port ethdev_port_id is 1 / end
      actions sample ratio 2 index 0 / vxlan_encap /
      represented_port ethdev_port_id 0 / end

The rule is inserted via port 0 (assumed to have "transfer" privilege).

Sample integrity rules
~~~~~~~~~~~~~~~~~~~~~~

Integrity rules can be created by the following commands:

Integrity rule that forwards valid TCP packets to group 1.
TCP packet integrity is matched with the ``l4_ok`` bit 3.

::

 testpmd> flow create 0 ingress
            pattern eth / ipv4 / tcp / integrity value mask 8 value spec 8 / end
            actions jump group 1 / end

Integrity rule that forwards invalid packets to application.
General packet integrity is matched with the ``packet_ok`` bit 0.

::

 testpmd> flow create 0 ingress pattern integrity value mask 1 value spec 0 / end actions queue index 0 / end

Sample conntrack rules
~~~~~~~~~~~~~~~~~~~~~~

Conntrack rules can be set by the following commands

Need to construct the connection context with provided information.
In the first table, create a flow rule by using conntrack action and jump to
the next table. In the next table, create a rule to check the state.

::

 testpmd> set conntrack com peer 1 is_orig 1 enable 1 live 1 sack 1 cack 0
        last_dir 0 liberal 0 state 1 max_ack_win 7 r_lim 5 last_win 510
        last_seq 2632987379 last_ack 2532480967 last_end 2632987379
        last_index 0x8
 testpmd> set conntrack orig scale 7 fin 0 acked 1 unack_data 0
        sent_end 2632987379 reply_end 2633016339 max_win 28960
        max_ack 2632987379
 testpmd> set conntrack rply scale 7 fin 0 acked 1 unack_data 0
        sent_end 2532480967 reply_end 2532546247 max_win 65280
        max_ack 2532480967
 testpmd> flow indirect_action 0 create ingress action conntrack / end
 testpmd> flow create 0 group 3 ingress pattern eth / ipv4 / tcp / end actions indirect 0 / jump group 5 / end
 testpmd> flow create 0 group 5 ingress pattern eth / ipv4 / tcp / conntrack is 1 / end actions queue index 5 / end

Construct the conntrack again with only "is_orig" set to 0 (other fields are
ignored), then use "update" interface to update the direction. Create flow
rules like above for the peer port.

::

 testpmd> flow indirect_action 0 update 0 action conntrack_update dir / end

Sample meter with policy rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Meter with policy rules can be created by the following commands:

Need to create policy first and actions are set for green/yellow/red colors.
Create meter with policy id. Create flow with meter id.

Example for policy with meter color action. The purpose is to color the packet
to reflect the meter color result.
The meter policy action list: ``green -> green, yellow -> yellow, red -> red``.

::

   testpmd> add port meter profile srtcm_rfc2697 0 13 21504 2688 0 0
   testpmd> add port meter policy 0 1 g_actions color type green / end y_actions color type yellow / end
            r_actions color type red / end
   testpmd> create port meter 0 1 13 1 yes 0xffff 0 0
   testpmd> flow create 0 priority 0 ingress group 1 pattern eth / end actions meter mtr_id 1 / end

Sample L2TPv2 RSS rules
~~~~~~~~~~~~~~~~~~~~~~~

L2TPv2 RSS rules can be created by the following commands::

   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 type control
            / end actions rss types l2tpv2 end queues end / end
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 / end
            actions rss types eth l2-src-only end queues end / end
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 / ppp / end
            actions rss types l2tpv2 end queues end / end
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 / ppp / ipv4
            / end actions rss types ipv4 end queues end / end
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 / ppp / ipv6
            / udp / end actions rss types ipv6-udp end queues end / end
   testpmd> flow create 0 ingress pattern eth / ipv6 / udp / l2tpv2 / ppp / ipv4
            / tcp / end actions rss types ipv4-tcp end queues end / end
   testpmd> flow create 0 ingress pattern eth / ipv6 / udp / l2tpv2 / ppp / ipv6
            / end actions rss types ipv6 end queues end / end

Sample L2TPv2 FDIR rules
~~~~~~~~~~~~~~~~~~~~~~~~

L2TPv2 FDIR rules can be created by the following commands::

   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 type control
            session_id is 0x1111 / end actions queue index 3 / end
   testpmd> flow create 0 ingress pattern eth src is 00:00:00:00:00:01 / ipv4
            / udp / l2tpv2 type data / end actions queue index 3 / end
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 type data
            session_id is 0x1111 / ppp / end actions queue index 3 / end
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 / ppp / ipv4
            src is 10.0.0.1 / end actions queue index 3 / end
   testpmd> flow create 0 ingress pattern eth / ipv6 / udp / l2tpv2 / ppp / ipv6
            dst is ABAB:910B:6666:3457:8295:3333:1800:2929 / end actions queue index 3 / end
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 / ppp / ipv4
            / udp src is 22 / end actions queue index 3 / end
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / l2tpv2 / ppp / ipv4
            / tcp dst is 23 / end actions queue index 3 / end

Sample RAW rule
~~~~~~~~~~~~~~~

A RAW rule can be created as following using ``pattern_hex`` key and mask.

::

    testpmd> flow create 0 group 0 priority 1 ingress pattern raw relative is 0 search is 0 offset
             is 0 limit is 0 pattern_hex spec 00000000000000000000000000000000000000000000000000000a0a0a0a
             pattern_hex mask 0000000000000000000000000000000000000000000000000000ffffffff / end actions
             queue index 4 / end

BPF Functions
--------------

The following sections show functions to load/unload eBPF based filters.

bpf-load
~~~~~~~~

Load an eBPF program as a callback for particular RX/TX queue::

   testpmd> bpf-load rx|tx (portid) (queueid) (load-flags) (bpf-prog-filename)

The available load-flags are:

* ``J``: use JIT generated native code, otherwise BPF interpreter will be used.

* ``M``: assume input parameter is a pointer to rte_mbuf, otherwise assume it is a pointer to first segment's data.

* ``-``: none.

.. note::

   You'll need clang v3.7 or above to build bpf program you'd like to load

For example:

.. code-block:: console

   cd examples/bpf
   clang -O2 -target bpf -c t1.c

Then to load (and JIT compile) t1.o at RX queue 0, port 1:

.. code-block:: console

   testpmd> bpf-load rx 1 0 J ./dpdk.org/examples/bpf/t1.o

To load (not JITed) t1.o at TX queue 0, port 0:

.. code-block:: console

   testpmd> bpf-load tx 0 0 - ./dpdk.org/examples/bpf/t1.o

bpf-unload
~~~~~~~~~~

Unload previously loaded eBPF program for particular RX/TX queue::

   testpmd> bpf-unload rx|tx (portid) (queueid)

For example to unload BPF filter from TX queue 0, port 0:

.. code-block:: console

   testpmd> bpf-unload tx 0 0

Flex Item Functions
-------------------

The following sections show functions that configure and create flex item object,
create flex pattern and use it in a flow rule.
The commands will use 20 bytes IPv4 header for examples:

::

   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ver  |  IHL  |     TOS       |        length                 | +0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       identification          | flg |    frag. offset         | +4
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       TTL     |  protocol     |        checksum               | +8
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               source IP address                               | +12
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |              destination IP address                           | +16
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


Create flex item
~~~~~~~~~~~~~~~~

Flex item object is created by PMD according to a new header configuration. The
header configuration is compiled by the testpmd and stored in
``rte_flow_item_flex_conf`` type variable.

::

   # flow flex_item create <port> <flex id> <configuration file>
   testpmd> flow flex_item init 0 3 ipv4_flex_config.json
   port-0: created flex item #3

Flex item configuration is kept in external JSON file.
It describes the following header elements:

**New header length.**

Specify whether the new header has fixed or variable length and the basic/minimal
header length value.

If header length is not fixed, header location with a value that completes header
length calculation and scale/offset function must be added.

Scale function depends on port hardware.

**Next protocol.**

Describes location in the new header that specify following network header type.

**Flow match samples.**

Describes locations in the new header that will be used in flow rules.

Number of flow samples and sample maximal length depend of port hardware.

**Input trigger.**

Describes preceding network header configuration.

**Output trigger.**

Describes conditions that trigger transfer to following network header

.. code-block:: json

   {
      "next_header": { "field_mode": "FIELD_MODE_FIXED", "field_size": 20},
      "next_protocol": {"field_size": 8, "field_base": 72},
      "sample_data": [
         { "field_mode": "FIELD_MODE_FIXED", "field_size": 32, "field_base": 0},
         { "field_mode": "FIELD_MODE_FIXED", "field_size": 32, "field_base": 32},
         { "field_mode": "FIELD_MODE_FIXED", "field_size": 32, "field_base": 64},
         { "field_mode": "FIELD_MODE_FIXED", "field_size": 32, "field_base": 96}
      ],
      "input_link": [
         {"item": "eth type is 0x0800"},
         {"item": "vlan inner_type is 0x0800"}
      ],
      "output_link": [
         {"item": "udp", "next": 17},
         {"item": "tcp", "next": 6},
         {"item": "icmp", "next": 1}
      ]
   }


Flex pattern and flow rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Flex pattern describe parts of network header that will trigger flex flow item hit in a flow rule.
Flex pattern directly related to flex item samples configuration.
Flex pattern can be shared between ports.

**Flex pattern and flow rule to match IPv4 version and 20 bytes length**

::

   # set flex_pattern <pattern_id> is <hex bytes sequence>
   testpmd> flow flex_item pattern 5 is 45FF
   created pattern #5

   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / flex item is 3 pattern is 5 / end actions mark id 1 / queue index 0 / end
   Flow rule #0 created

**Flex pattern and flow rule to match packets with source address 1.2.3.4**

::

   testpmd> flow flex_item pattern 2 spec 45000000000000000000000001020304 mask FF0000000000000000000000FFFFFFFF
   created pattern #2

   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / flex item is 3 pattern is 2 / end actions mark id 1 / queue index 0 / end
   Flow rule #0 created

Driver specific commands
------------------------

Some drivers provide specific features.
See:

- :ref:`net/bonding testpmd driver specific commands <bonding_testpmd_commands>`
- :ref:`net/i40e testpmd driver specific commands <net_i40e_testpmd_commands>`
- :ref:`net/ixgbe testpmd driver specific commands <net_ixgbe_testpmd_commands>`
