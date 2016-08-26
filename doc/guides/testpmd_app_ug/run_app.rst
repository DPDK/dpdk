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

Running the Application
=======================

EAL Command-line Options
------------------------

The following are the EAL command-line options that can be used in conjunction with the testpmd,
or any other DPDK application.
See the DPDK Getting Started Guides for more information on these options.

*   ``-c COREMASK``

    Set the hexadecimal bitmask of the cores to run on.

*   ``-l CORELIST``

    List of cores to run on

    The argument format is ``<c1>[-c2][,c3[-c4],...]``
    where ``c1``, ``c2``, etc are core indexes between 0 and 128.

*   ``--lcores COREMAP``

    Map lcore set to physical cpu set

    The argument format is::

       <lcores[@cpus]>[<,lcores[@cpus]>...]

    Lcore and CPU lists are grouped by ``(`` and ``)`` Within the group.
    The ``-`` character is used as a range separator and ``,`` is used as a single number separator.
    The grouping ``()`` can be omitted for single element group.
    The ``@`` can be omitted if cpus and lcores have the same value.

*   ``--master-lcore ID``

    Core ID that is used as master.

*   ``-n NUM``

    Set the number of memory channels to use.

*   ``-b, --pci-blacklist domain:bus:devid.func``

    Blacklist a PCI devise to prevent EAL from using it. Multiple -b options are allowed.

*   ``-d LIB.so``

    Load an external driver. Multiple -d options are allowed.

*   ``-w, --pci-whitelist domain:bus:devid:func``

    Add a PCI device in white list.

*   ``-m MB``

    Memory to allocate. See also ``--socket-mem``.

*   ``-r NUM``

    Set the number of memory ranks (auto-detected by default).

*   ``-v``

    Display the version information on startup.

*   ``--xen-dom0``

    Support application running on Xen Domain0 without hugetlbfs.

*   ``--syslog``

    Set the syslog facility.

*   ``--socket-mem``

    Set the memory to allocate on specific sockets (use comma separated values).

*   ``--huge-dir``

    Specify the directory where the hugetlbfs is mounted.

*   ``--proc-type``

    Set the type of the current process.

*   ``--file-prefix``

    Prefix for hugepage filenames.

*   ``-vmware-tsc-map``

    Use VMware TSC map instead of native RDTSC.

*   ``--vdev``

    Add a virtual device using the format::

       <driver><id>[,key=val, ...]

    For example::

       --vdev 'net_pcap0,rx_pcap=input.pcap,tx_pcap=output.pcap'

*   ``--base-virtaddr``

    Specify base virtual address.

*   ``--create-uio-dev``

    Create ``/dev/uioX`` (usually done by hotplug).

*   ``--no-shconf``

    No shared config (mmap-ed files).

*   ``--no-pci``

    Disable pci.

*   ``--no-hpet``

    Disable hpet.

*   ``--no-huge``

    Use malloc instead of hugetlbfs.


Testpmd Command-line Options
----------------------------

The following are the command-line options for the testpmd applications.
They must be separated from the EAL options, shown in the previous section, with a ``--`` separator:

.. code-block:: console

    sudo ./testpmd -c 0xF -n 4 -- -i --portmask=0x1 --nb-cores=2

The commandline options are:

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

    Enable NUMA-aware allocation of RX/TX rings and of RX memory buffers (mbufs).

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

*   ``--crc-strip``

    Enable hardware CRC stripping.

*   ``--enable-lro``

    Enable large receive offload.

*   ``--enable-rx-cksum``

    Enable hardware RX checksum offload.

*   ``--enable-scatter``

    Enable scatter (multi-segment) RX.

*   ``--disable-hw-vlan``

    Disable hardware VLAN.

*   ``--disable-hw-vlan-filter``

    Disable hardware VLAN filter.

*   ``--disable-hw-vlan-strip``

    Disable hardware VLAN strip.

*   ``--disable-hw-vlan-extend``

    Disable hardware VLAN extend.

*   ``--enable-drop-en``

    Enable per-queue packet drop for packets with no descriptors.

*   ``--disable-rss``

    Disable RSS (Receive Side Scaling).

*   ``--port-topology=mode``

    Set port topology, where mode is ``paired`` (the default) or ``chained``.

    In ``paired`` mode, the forwarding is between pairs of ports, for example: (0,1), (2,3), (4,5).

    In ``chained`` mode, the forwarding is to the next available port in the port mask, for example: (0,1), (1,2), (2,0).

    The ordering of the ports can be changed using the portlist testpmd runtime function.

*   ``--forward-mode=mode``

    Set the forwarding mode where ``mode`` is one of the following::

       io (the default)
       mac
       mac_swap
       flowgen
       rxonly
       txonly
       csum
       icmpecho
       ieee1588

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
    The default value is 16.

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

*   ``--txqflags=0xXXXXXXXX``

    Set the hexadecimal bitmask of TX queue flags, where 0 <= N <= 0x7FFFFFFF.
    The default value is 0.

    .. note::

       When using hardware offload functions such as vlan or checksum
       add ``txqflags=0`` to force the full-featured TX code path.
       In some PMDs this may already be the default.


*   ``--rx-queue-stats-mapping=(port,queue,mapping)[,(port,queue,mapping)]``

    Set the RX queues statistics counters mapping 0 <= mapping <= 15.

*   ``--tx-queue-stats-mapping=(port,queue,mapping)[,(port,queue,mapping)]``

    Set the TX queues statistics counters mapping 0 <= mapping <= 15.

*   ``--no-flush-rx``

    Don't flush the RX streams before starting forwarding. Used mainly with the PCAP PMD.

*   ``--txpkts=X[,Y]``

    Set TX segment sizes or total packet length. Valid for ``tx-only``
    and ``flowgen`` forwarding modes.

*   ``--disable-link-check``

    Disable check on link status when starting/stopping ports.
