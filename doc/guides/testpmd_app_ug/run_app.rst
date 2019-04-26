..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Running the Application
=======================

EAL Command-line Options
------------------------

Please refer to  :doc:`../linux_gsg/linux_eal_parameters` or
:doc:`../freebsd_gsg/freebsd_eal_parameters` for a list of available EAL
command-line options.


Testpmd Command-line Options
----------------------------

The following are the command-line options for the testpmd applications.
They must be separated from the EAL options, shown in the previous section, with a ``--`` separator:

.. code-block:: console

    sudo ./testpmd -l 0-3 -n 4 -- -i --portmask=0x1 --nb-cores=2

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

*   ``--nb-cores=N``

    Set the number of forwarding cores,
    where 1 <= N <= "number of cores" or ``CONFIG_RTE_MAX_LCORE`` from the configuration file.
    The default value is 1.

*   ``--nb-ports=N``

    Set the number of forwarding ports,
    where 1 <= N <= "number of ports" on the board or ``CONFIG_RTE_MAX_ETHPORTS`` from the configuration file.
    The default value is the number of ports on the board.

*   ``--coremask=0xXX``

    Set the hexadecimal bitmask of the cores running the packet forwarding test.
    The master lcore is reserved for command line parsing only and cannot be masked on for packet forwarding.

*   ``--portmask=0xXX``

    Set the hexadecimal bitmask of the ports used by the packet forwarding test.

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

*   ``--mbuf-size=N``

    Set the data size of the mbufs used to N bytes, where N < 65536. The default value is 2048.

*   ``--total-num-mbufs=N``

    Set the number of mbufs to be allocated in the mbuf pools, where N > 1024.

*   ``--max-pkt-len=N``

    Set the maximum packet size to N bytes, where N >= 64. The default value is 1518.

*   ``--eth-peers-configfile=name``

    Use a configuration file containing the Ethernet addresses of the peer ports.
    The configuration file should contain the Ethernet addresses on separate lines::

       XX:XX:XX:XX:XX:01
       XX:XX:XX:XX:XX:02
       ...

*   ``--eth-peer=N,XX:XX:XX:XX:XX:XX``

    Set the MAC address ``XX:XX:XX:XX:XX:XX`` of the peer port N,
    where 0 <= N < ``CONFIG_RTE_MAX_ETHPORTS`` from the configuration file.

*   ``--tx-ip=SRC,DST``

    Set the source and destination IP address used when doing transmit only test.
    The defaults address values are source 192.18.0.1 and
    destination 192.18.0.2. These are special purpose addresses
    reserved for benchmarking (RFC 2544).

*   ``--tx-udp=SRC[,DST]``

    Set the source and destination UDP port number for transmit test only test.
    The default port is the port 9 which is defined for the discard protocol
    (RFC 863).

*   ``--pkt-filter-mode=mode``

    Set Flow Director mode where mode is either ``none`` (the default), ``signature`` or ``perfect``.
    See :ref:`testpmd_flow_director` for more details.

*   ``--pkt-filter-report-hash=mode``

    Set Flow Director hash match reporting mode where mode is ``none``, ``match`` (the default) or ``always``.

*   ``--pkt-filter-size=N``

    Set Flow Director allocated memory size, where N is 64K, 128K or 256K.
    Sizes are in kilobytes. The default is 64.

*   ``--pkt-filter-flexbytes-offset=N``

    Set the flexbytes offset.
    The offset is defined in words (not bytes) counted from the first byte of the destination Ethernet MAC address,
    where N is 0 <= N <= 32.
    The default value is 0x6.

*   ``--pkt-filter-drop-queue=N``

    Set the drop-queue.
    In perfect filter mode, when a rule is added with queue = -1, the packet will be enqueued into the RX drop-queue.
    If the drop-queue does not exist, the packet is dropped. The default value is N=127.

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

*   ``--burst=N``

    Set the number of packets per burst to N, where 1 <= N <= 512.
    The default value is 32.
    If set to 0, driver default is used if defined. Else, if driver
    default is not defined, default of 32 is used.

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

*   ``--rx-queue-stats-mapping=(port,queue,mapping)[,(port,queue,mapping)]``

    Set the RX queues statistics counters mapping 0 <= mapping <= 15.

*   ``--tx-queue-stats-mapping=(port,queue,mapping)[,(port,queue,mapping)]``

    Set the TX queues statistics counters mapping 0 <= mapping <= 15.

*   ``--no-flush-rx``

    Don't flush the RX streams before starting forwarding. Used mainly with the PCAP PMD.

*   ``--txpkts=X[,Y]``

    Set TX segment sizes or total packet length. Valid for ``tx-only``
    and ``flowgen`` forwarding modes.

*   ``--txonly-multi-flow``

    Generate multiple flows in txonly mode.

*   ``--disable-link-check``

    Disable check on link status when starting/stopping ports.

*   ``--no-lsc-interrupt``

    Disable LSC interrupts for all ports, even those supporting it.

*   ``--no-rmv-interrupt``

    Disable RMV interrupts for all ports, even those supporting it.

*   ``--bitrate-stats=N``

    Set the logical core N to perform bitrate calculation.

*   ``--print-event <unknown|intr_lsc|queue_state|intr_reset|vf_mbox|macsec|intr_rmv|dev_probed|dev_released|all>``

    Enable printing the occurrence of the designated event. Using all will
    enable all of them.

*   ``--mask-event <unknown|intr_lsc|queue_state|intr_reset|vf_mbox|macsec|intr_rmv|dev_probed|dev_released|all>``

    Disable printing the occurrence of the designated event. Using all will
    disable all of them.

*   ``--flow-isolate-all``

    Providing this parameter requests flow API isolated mode on all ports at
    initialization time. It ensures all traffic is received through the
    configured flow rules only (see flow command).

    Ports that do not support this mode are automatically discarded.

*   ``--tx-offloads=0xXXXXXXXX``

    Set the hexadecimal bitmask of TX queue offloads.
    The default value is 0.

*   ``--hot-plug``

    Enable device event monitor mechanism for hotplug.

*   ``--vxlan-gpe-port=N``

    Set the UDP port number of tunnel VXLAN-GPE to N.
    The default value is 4790.

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
