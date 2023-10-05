..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Running the Application
=======================

EAL Command-line Options
------------------------

Please refer to :doc:`EAL parameters (Linux) <../linux_gsg/linux_eal_parameters>`
or :doc:`EAL parameters (FreeBSD) <../freebsd_gsg/freebsd_eal_parameters>` for
a list of available EAL command-line options.


Testpmd Command-line Options
----------------------------

The following are the command-line options for the testpmd applications.
They must be separated from the EAL options, shown in the previous section, with a ``--`` separator:

.. code-block:: console

    sudo ./dpdk-testpmd -l 0-3 -n 4 -- -i --portmask=0x1 --nb-cores=2

The command line options are:

*   ``-i, --interactive``

    Run testpmd in interactive mode.
    In this mode, the testpmd starts with a prompt that can be used to start and stop forwarding,
    configure the application and display stats on the current packet processing session.
    See :ref:`testpmd_runtime` for more details.

    In non-interactive mode,
    the application starts with the configuration specified on the command-line and
    immediately enters forwarding mode.

*   ``-h, --help``

    Display a help message and quit.

*   ``-a, --auto-start``

    Start forwarding on initialization.

*   ``--tx-first``

    Start forwarding, after sending a burst of packets first.

.. Note::

   This flag should be only used in non-interactive mode.

*   ``--stats-period PERIOD``

    Display statistics every PERIOD seconds, if interactive mode is disabled.
    The default value is 0, which means that the statistics will not be displayed.

*   ``--display-xstats xstat_name1[,...]``

    Display comma-separated list of extended statistics every PERIOD seconds
    as specified in ``--stats-period`` or when used with interactive commands
    that show Rx/Tx statistics (i.e. 'show port stats').

*   ``--nb-cores=N``

    Set the number of forwarding cores,
    where 1 <= N <= "number of cores" or ``RTE_MAX_LCORE`` from the configuration file.
    The default value is 1.

*   ``--nb-ports=N``

    Set the number of forwarding ports,
    where 1 <= N <= "number of ports" on the board or ``RTE_MAX_ETHPORTS`` from the configuration file.
    The default value is the number of ports on the board.

*   ``--coremask=0xXX``

    Set the hexadecimal bitmask of the cores running the packet forwarding test.
    The main lcore is reserved for command line parsing only and cannot be masked on for packet forwarding.

*   ``--portmask=0xXX``

    Set the hexadecimal bitmask of the ports used by the packet forwarding test.

*   ``--portlist=X``

      Set the forwarding ports based on the user input used by the packet forwarding test.
      '-' denotes a range of ports to set including the two specified port IDs
      ',' separates multiple port values.
      Possible examples like --portlist=0,1 or --portlist=0-2 or --portlist=0,1-2 etc

*   ``--numa``

    Enable NUMA-aware allocation of RX/TX rings and of RX memory buffers
    (mbufs). [Default setting]

*   ``--no-numa``

    Disable NUMA-aware allocation of RX/TX rings and of RX memory buffers (mbufs).

*   ``--port-numa-config=(port,socket)[,(port,socket)]``

    Specify the socket on which the memory pool to be used by the port will be allocated.

*   ``--ring-numa-config=(port,flag,socket)[,(port,flag,socket)]``

    Specify the socket on which the TX/RX rings for the port will be allocated.
    Where flag is 1 for RX, 2 for TX, and 3 for RX and TX.

*   ``--socket-num=N``

    Set the socket from which all memory is allocated in NUMA mode,
    where 0 <= N < number of sockets on the board.

*   ``--mbuf-size=N[,N1[,...Nn]``

    Set the data size of the mbufs used to N bytes, where N < 65536.
    The default value is 2048. If multiple mbuf-size values are specified the
    extra memory pools will be created for allocating mbufs to receive packets
    with buffer splitting features.

*   ``--total-num-mbufs=N``

    Set the number of mbufs to be allocated in the mbuf pools, where N > 1024.

*   ``--max-pkt-len=N``

    Set the maximum packet size to N bytes, where N >= 64. The default value is 1518.

*   ``--max-lro-pkt-size=N``

    Set the maximum LRO aggregated packet size to N bytes, where N >= 64.

*   ``--eth-peers-configfile=name``

    Use a configuration file containing the Ethernet addresses of the peer ports.
    The configuration file should contain the Ethernet addresses on separate lines::

       XX:XX:XX:XX:XX:01
       XX:XX:XX:XX:XX:02
       ...

*   ``--eth-peer=N,XX:XX:XX:XX:XX:XX``

    Set the MAC address ``XX:XX:XX:XX:XX:XX`` of the peer port N,
    where 0 <= N < ``RTE_MAX_ETHPORTS``.

*   ``--tx-ip=SRC,DST``

    Set the source and destination IP address used when doing transmit only test.
    The defaults address values are source 198.18.0.1 and
    destination 198.18.0.2. These are special purpose addresses
    reserved for benchmarking (RFC 5735).

*   ``--tx-udp=SRC[,DST]``

    Set the source and destination UDP port number for transmit test only test.
    The default port is the port 9 which is defined for the discard protocol
    (RFC 863).

*   ``--disable-crc-strip``

    Disable hardware CRC stripping.

*   ``--enable-lro``

    Enable large receive offload.

*   ``--enable-rx-cksum``

    Enable hardware RX checksum offload.

*   ``--enable-scatter``

    Enable scatter (multi-segment) RX.

*   ``--enable-hw-vlan``

    Enable hardware VLAN.

*   ``--enable-hw-vlan-filter``

    Enable hardware VLAN filter.

*   ``--enable-hw-vlan-strip``

    Enable hardware VLAN strip.

*   ``--enable-hw-vlan-extend``

    Enable hardware VLAN extend.

*   ``--enable-hw-qinq-strip``

    Enable hardware QINQ strip.

*   ``--enable-drop-en``

    Enable per-queue packet drop for packets with no descriptors.

*   ``--disable-rss``

    Disable RSS (Receive Side Scaling).

*   ``--port-topology=mode``

    Set port topology, where mode is ``paired`` (the default), ``chained`` or ``loop``.

    In ``paired`` mode, the forwarding is between pairs of ports, for example: (0,1), (2,3), (4,5).

    In ``chained`` mode, the forwarding is to the next available port in the port mask, for example: (0,1), (1,2), (2,0).

    The ordering of the ports can be changed using the portlist testpmd runtime function.

    In ``loop`` mode, ingress traffic is simply transmitted back on the same interface.

*   ``--forward-mode=mode``

    Set the forwarding mode where ``mode`` is one of the following::

       io (the default)
       mac
       macswap
       flowgen
       rxonly
       txonly
       csum
       icmpecho
       ieee1588
       tm
       noisy
       5tswap
       shared-rxq
       recycle_mbufs

*   ``--rss-ip``

    Set RSS functions for IPv4/IPv6 only.

*   ``--rss-udp``

    Set RSS functions for IPv4/IPv6 and UDP.

*   ``--rxq=N``

    Set the number of RX queues per port to N, where 1 <= N <= 65535.
    The default value is 1.

*   ``--rxd=N``

    Set the number of descriptors in the RX rings to N, where N > 0.
    The default value is 128.

*   ``--txq=N``

    Set the number of TX queues per port to N, where 1 <= N <= 65535.
    The default value is 1.

*   ``--txd=N``

    Set the number of descriptors in the TX rings to N, where N > 0.
    The default value is 512.

*   ``--hairpinq=N``

    Set the number of hairpin queues per port to N, where 1 <= N <= 65535.
    The default value is 0. The number of hairpin queues are added to the
    number of TX queues and to the number of RX queues. then the first
    RX hairpin is binded to the first TX hairpin, the second RX hairpin is
    binded to the second TX hairpin and so on. The index of the first
    RX hairpin queue is the number of RX queues as configured using --rxq.
    The index of the first TX hairpin queue is the number of TX queues
    as configured using --txq.

*   ``--burst=N``

    Set the number of packets per burst to N, where 1 <= N <= 512.
    The default value is 32.
    If set to 0, driver default is used if defined. Else, if driver
    default is not defined, default of 32 is used.

*   ``--flowgen-clones=N``

    Set the number of each packet clones to be sent in `flowgen` mode.
    Sending clones reduces host CPU load on creating packets and may help
    in testing extreme speeds or maxing out Tx packet performance.
    N should be not zero, but less than 'burst' parameter.

*   ``--flowgen-flows=N``

    Set the number of flows to be generated in `flowgen` mode, where
    1 <= N <= INT32_MAX.

*   ``--mbcache=N``

    Set the cache of mbuf memory pools to N, where 0 <= N <= 512.
    The default value is 16.

*   ``--rxpt=N``

    Set the prefetch threshold register of RX rings to N, where N >= 0.
    The default value is 8.

*   ``--rxht=N``

    Set the host threshold register of RX rings to N, where N >= 0.
    The default value is 8.

*   ``--rxfreet=N``

    Set the free threshold of RX descriptors to N, where 0 <= N < value of --rxd.
    The default value is 0.

*   ``--rxwt=N``

    Set the write-back threshold register of RX rings to N, where N >= 0.
    The default value is 4.

*   ``--txpt=N``

    Set the prefetch threshold register of TX rings to N, where N >= 0.
    The default value is 36.

*   ``--txht=N``

    Set the host threshold register of TX rings to N, where N >= 0.
    The default value is 0.

*   ``--txwt=N``

    Set the write-back threshold register of TX rings to N, where N >= 0.
    The default value is 0.

*   ``--txfreet=N``

    Set the transmit free threshold of TX rings to N, where 0 <= N <= value of ``--txd``.
    The default value is 0.

*   ``--txrst=N``

    Set the transmit RS bit threshold of TX rings to N, where 0 <= N <= value of ``--txd``.
    The default value is 0.

*   ``--no-flush-rx``

    Don't flush the RX streams before starting forwarding. Used mainly with the PCAP PMD.

*   ``--rxoffs=X[,Y]``

    Set the offsets of packet segments on receiving if split
    feature is engaged. Affects only the queues configured
    with split offloads (currently BUFFER_SPLIT is supported only).

*   ``--rxpkts=X[,Y]``

    Set the length of segments to scatter packets on receiving if split
    feature is engaged. Affects only the queues configured
    with split offloads (currently BUFFER_SPLIT is supported only).
    Optionally the multiple memory pools can be specified with --mbuf-size
    command line parameter and the mbufs to receive will be allocated
    sequentially from these extra memory pools.

*   ``--txpkts=X[,Y]``

    Set TX segment sizes or total packet length. Valid for ``tx-only``
    and ``flowgen`` forwarding modes.

* ``--multi-rx-mempool``

    Enable multiple mbuf pools per Rx queue.

*   ``--txonly-multi-flow``

    Generate multiple flows in txonly mode.

*   ``--rxq-share=[X]``

    Create queues in shared Rx queue mode if device supports.
    Shared Rx queues are grouped per X ports. X defaults to UINT32_MAX,
    implies all ports join share group 1. Forwarding engine "shared-rxq"
    should be used for shared Rx queues. This engine does Rx only and
    update stream statistics accordingly.

*   ``--eth-link-speed``

    Set a forced link speed to the ethernet port::

       10 - 10Mbps (not supported)
       100 - 100Mbps (not supported)
       1000 - 1Gbps
       2500 - 2.5Gbps
       5000 - 5Gbps
       10000 - 10Gbps
       25000 - 25Gbps
       40000 - 40Gbps
       50000 - 50Gbps
       100000 - 100Gbps
       200000 - 200Gbps
       400000 - 400Gbps
       ...

*   ``--disable-link-check``

    Disable check on link status when starting/stopping ports.

*   ``--disable-device-start``

    Do not automatically start all ports. This allows testing
    configuration of rx and tx queues before device is started
    for the first time.

*   ``--no-lsc-interrupt``

    Disable LSC interrupts for all ports, even those supporting it.

*   ``--no-rmv-interrupt``

    Disable RMV interrupts for all ports, even those supporting it.

*   ``--bitrate-stats=N``

    Set the logical core N to perform bitrate calculation.

*   ``--print-event <unknown|intr_lsc|queue_state|intr_reset|vf_mbox|macsec|intr_rmv|dev_probed|dev_released|flow_aged|err_recovering|recovery_success|recovery_failed|all>``

    Enable printing the occurrence of the designated event. Using all will
    enable all of them.

*   ``--mask-event <unknown|intr_lsc|queue_state|intr_reset|vf_mbox|macsec|intr_rmv|dev_probed|dev_released|flow_aged|err_recovering|recovery_success|recovery_failed|all>``

    Disable printing the occurrence of the designated event. Using all will
    disable all of them.

*   ``--flow-isolate-all``

    Providing this parameter requests flow API isolated mode on all ports at
    initialization time. It ensures all traffic is received through the
    configured flow rules only (see flow command).

    Ports that do not support this mode are automatically discarded.

*   ``--disable-flow-flush``

    Disable port flow flush when stopping port.
    This allows testing keep flow rules or shared flow objects across restart.

*   ``--tx-offloads=0xXXXXXXXX``

    Set the hexadecimal bitmask of TX queue offloads.
    The default value is 0.

*   ``--rx-offloads=0xXXXXXXXX``

    Set the hexadecimal bitmask of RX queue offloads.
    The default value is 0.

*   ``--hot-plug``

    Enable device event monitor mechanism for hotplug.

*   ``--vxlan-gpe-port=N``

    Set the UDP port number of tunnel VXLAN-GPE to N.
    The default value is 4790.

*   ``--geneve-parsed-port=N``

    Set the UDP port number that is used for parsing the GENEVE protocol to N.
    HW may be configured with another tunnel Geneve port.
    The default value is 6081.

*   ``--mlockall``

    Enable locking all memory.

*   ``--no-mlockall``

    Disable locking all memory.

*   ``--mp-alloc <native|anon|xmem|xmemhuge>``

    Select mempool allocation mode:

    * native: create and populate mempool using native DPDK memory
    * anon: create mempool using native DPDK memory, but populate using
      anonymous memory
    * xmem: create and populate mempool using externally and anonymously
      allocated area
    * xmemhuge: create and populate mempool using externally and anonymously
      allocated hugepage area

*   ``--noisy-forward-mode=mode``

    Set the noisy vnf forwarding mode where ``mode`` is one of the following::

       io (the default)
       mac
       macswap
       5tswap

*   ``--noisy-tx-sw-buffer-size``

    Set the number of maximum elements  of the FIFO queue to be created
    for buffering packets. Only available with the noisy forwarding mode.
    The default value is 0.

*   ``--noisy-tx-sw-buffer-flushtime=N``

    Set the time before packets in the FIFO queue is flushed.
    Only available with the noisy forwarding mode. The default value is 0.

*   ``--noisy-lkup-memory=N``

    Set the size of the noisy neighbor simulation memory buffer in MB to N.
    Only available with the noisy forwarding mode. The default value is 0.


*   ``--noisy-lkup-num-reads=N``

    Set the number of reads to be done in noisy neighbor simulation memory buffer to N.
    Only available with the noisy forwarding mode. The default value is 0.

*   ``--noisy-lkup-num-writes=N``

    Set the number of writes to be done in noisy neighbor simulation memory buffer to N.
    Only available with the noisy forwarding mode. The default value is 0.

*   ``--noisy-lkup-num-reads-writes=N``

    Set the number of r/w accesses to be done in noisy neighbor simulation memory buffer to N.
    Only available with the noisy forwarding mode. The default value is 0.

*   ``--no-iova-contig``

    Enable to create mempool which is not IOVA contiguous. Valid only with --mp-alloc=anon.
    The default value is 0.

*   ``--rx-mq-mode``

    Set the hexadecimal bitmask of RX multi queue mode which can be enabled.
    The default value is 0x7::

       RTE_ETH_MQ_RX_RSS_FLAG | RTE_ETH_MQ_RX_DCB_FLAG | RTE_ETH_MQ_RX_VMDQ_FLAG

*   ``--record-core-cycles``

    Enable measurement of CPU cycles per packet.

*   ``--record-burst-stats``

    Enable display of RX and TX burst stats.

*   ``--hairpin-mode=0xXXXX``

    Set the hairpin port configuration with bitmask, only valid when hairpin queues number is set::

	bit 18 - hairpin TX queues will use RTE memory
	bit 16 - hairpin TX queues will use locked device memory
	bit 13 - hairpin RX queues will use RTE memory
	bit 12 - hairpin RX queues will use locked device memory
	bit 9 - force memory settings of hairpin TX queue
	bit 8 - force memory settings of hairpin RX queue
	bit 4 - explicit Tx flow rule
	bit 1 - two hairpin ports paired
	bit 0 - two hairpin ports loop

    The default value is 0. Hairpin will use single port mode and implicit Tx flow mode.


Testpmd Multi-Process Command-line Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following are the command-line options for testpmd multi-process support:

*   primary process:

.. code-block:: console

    sudo ./dpdk-testpmd -a xxx --proc-type=auto -l 0-1 -- -i --rxq=4 --txq=4 \
        --num-procs=2 --proc-id=0

*   secondary process:

.. code-block:: console

    sudo ./dpdk-testpmd -a xxx --proc-type=auto -l 2-3 -- -i --rxq=4 --txq=4 \
        --num-procs=2 --proc-id=1

The command line options are:

*   ``--num-procs=N``

    The number of processes which will be used.

*   ``--proc-id=ID``

    The ID of the current process (ID < num-procs). ID should be different in
    primary process and secondary process, which starts from '0'.

Calculation rule for queue:
All queues are allocated to different processes based on ``proc_num`` and
``proc_id``.
Calculation rule for the testpmd to allocate queues to each process:
*   start(queue start id) = proc_id * nb_q / num_procs；

*   end(queue end id) = start + nb_q / num_procs；

For example, if testpmd is configured to have 4 Tx and Rx queues,
queues 0 and 1 will be used by the primary process and
queues 2 and 3 will be used by the secondary process.

The number of queues should be a multiple of the number of processes. If not,
redundant queues will exist after queues are allocated to processes. If RSS
is enabled, packet loss occurs when traffic is sent to all processes at the same
time. Some traffic goes to redundant queues and cannot be forwarded.

All the dev ops is supported in primary process. While secondary process is
not permitted to allocate or release shared memory, so some ops are not supported
as follows:

- ``dev_configure``
- ``dev_start``
- ``dev_stop``
- ``dev_reset``
- ``rx_queue_setup``
- ``tx_queue_setup``
- ``rx_queue_release``
- ``tx_queue_release``

So, any command from testpmd which calls those APIs will not be supported in
secondary process, like:

.. code-block:: console

    port config all rxq|txq|rxd|txd <value>
    port config <port_id> rx_offload xxx on/off
    port config <port_id> tx_offload xxx on/off

etc.

When secondary is running, port in primary is not permitted to be stopped.
Reconfigure operation is only valid in primary.

Stats is supported, stats will not change when one quits and starts, as they
share the same buffer to store the stats. Flow rules are maintained in process
level: primary and secondary has its own flow list (but one flow list in HW).
The two can see all the queues, so setting the flow rules for the other is OK.
But in the testpmd primary process receiving or transmitting packets from the
queue allocated for secondary process is not permitted, and same for secondary
process.

Flow API and RSS are supported.
