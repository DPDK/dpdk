..  BSD LICENSE
    Copyright(c) 2017 Intel Corporation. All rights reserved.
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

KNI Poll Mode Driver
======================

KNI PMD is wrapper to the :ref:`librte_kni <kni>` library.

This PMD enables using KNI without having a KNI specific application,
any forwarding application can use PMD interface for KNI.

Sending packets to any DPDK controlled interface or sending to the
Linux networking stack will be transparent to the DPDK application.

To create a KNI device ``net_kni#`` device name should be used, and this
will create ``kni#`` Linux virtual network interface.

There is no physical device backend for the virtual KNI device.

Packets sent to the KNI Linux interface will be received by the DPDK
application, and DPDK application may forward packets to a physical NIC
or to a virtual device (like another KNI interface or PCAP interface).

To forward any traffic from physical NIC to the Linux networking stack,
an application should control a physical port and create one virtual KNI port,
and forward between two.

Using this PMD requires KNI kernel module be inserted.


Usage
-----

EAL ``--vdev`` argument can be used to create KNI device instance, like::

        testpmd --vdev=net_kni0 --vdev=net_kn1 -- -i

Above command will create ``kni0`` and ``kni1`` Linux network interfaces,
those interfaces can be controlled by standard Linux tools.

When testpmd forwarding starts, any packets sent to ``kni0`` interface
forwarded to the ``kni1`` interface and vice versa.

There is no hard limit on number of interfaces that can be created.


Default interface configuration
-------------------------------

``librte_kni`` can create Linux network interfaces with different features,
feature set controlled by a configuration struct, and KNI PMD uses a fixed
configuration:

    .. code-block:: console

        Interface name: kni#
        force bind kernel thread to a core : NO
        mbuf size: MAX_PACKET_SZ

KNI control path is not supported with the PMD, since there is no physical
backend device by default.


PMD arguments
-------------

``no_request_thread``, by default PMD creates a phtread for each KNI interface
to handle Linux network interface control commands, like ``ifconfig kni0 up``

With ``no_request_thread`` option, pthread is not created and control commands
not handled by PMD.

By default request thread is enabled. And this argument should not be used
most of the time, unless this PMD used with customized DPDK application to handle
requests itself.

Argument usage::

        testpmd --vdev "net_kni0,no_request_thread=1" -- -i


PMD log messages
----------------

If KNI kernel module (rte_kni.ko) not inserted, following error log printed::

        "KNI: KNI subsystem has not been initialized. Invoke rte_kni_init() first"


PMD testing
-----------

It is possible to test PMD quickly using KNI kernel module loopback feature:

* Insert KNI kernel module with loopback support:

    .. code-block:: console

        insmod build/kmod/rte_kni.ko lo_mode=lo_mode_fifo_skb

* Start testpmd with no physical device but two KNI virtual devices:

    .. code-block:: console

        ./testpmd --vdev net_kni0 --vdev net_kni1 -- -i

    .. code-block:: console

        ...
        Configuring Port 0 (socket 0)
        KNI: pci: 00:00:00       c580:b8
        Port 0: 1A:4A:5B:7C:A2:8C
        Configuring Port 1 (socket 0)
        KNI: pci: 00:00:00       600:b9
        Port 1: AE:95:21:07:93:DD
        Checking link statuses...
        Port 0 Link Up - speed 10000 Mbps - full-duplex
        Port 1 Link Up - speed 10000 Mbps - full-duplex
        Done
        testpmd>

* Observe Linux interfaces

    .. code-block:: console

        $ ifconfig kni0 && ifconfig kni1
        kni0: flags=4098<BROADCAST,MULTICAST>  mtu 1500
                ether ae:8e:79:8e:9b:c8  txqueuelen 1000  (Ethernet)
                RX packets 0  bytes 0 (0.0 B)
                RX errors 0  dropped 0  overruns 0  frame 0
                TX packets 0  bytes 0 (0.0 B)
                TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0

        kni1: flags=4098<BROADCAST,MULTICAST>  mtu 1500
                ether 9e:76:43:53:3e:9b  txqueuelen 1000  (Ethernet)
                RX packets 0  bytes 0 (0.0 B)
                RX errors 0  dropped 0  overruns 0  frame 0
                TX packets 0  bytes 0 (0.0 B)
                TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0


* Start forwarding with tx_first:

    .. code-block:: console

        testpmd> start tx_first

* Quit and check forwarding stats:

    .. code-block:: console

        testpmd> quit
        Telling cores to stop...
        Waiting for lcores to finish...

        ---------------------- Forward statistics for port 0  ----------------------
        RX-packets: 35637905       RX-dropped: 0             RX-total: 35637905
        TX-packets: 35637947       TX-dropped: 0             TX-total: 35637947
        ----------------------------------------------------------------------------

        ---------------------- Forward statistics for port 1  ----------------------
        RX-packets: 35637915       RX-dropped: 0             RX-total: 35637915
        TX-packets: 35637937       TX-dropped: 0             TX-total: 35637937
        ----------------------------------------------------------------------------

        +++++++++++++++ Accumulated forward statistics for all ports+++++++++++++++
        RX-packets: 71275820       RX-dropped: 0             RX-total: 71275820
        TX-packets: 71275884       TX-dropped: 0             TX-total: 71275884
        ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

