..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation

..  bbdev_app:

Loop-back Sample Application using Baseband Device (bbdev)
==========================================================

The baseband sample application is a simple example of packet processing using
the Data Plane Development Kit (DPDK) for baseband workloads using Wireless
Device abstraction library.

Overview
--------

The Baseband device sample application performs a loop-back operation using a
baseband device capable of transceiving data packets.
A packet is received on an ethernet port -> enqueued for downlink baseband
operation -> dequeued from the downlink baseband device -> enqueued for uplink
baseband operation -> dequeued from the baseband device -> then the received
packet is compared with the baseband operations output. Then it's looped back to
the ethernet port.

*   The MAC header is preserved in the packet

Limitations
-----------

* Only one baseband device and one ethernet port can be used.

Compiling the Application
-------------------------

#. DPDK needs to be built with ``baseband_turbo_sw`` PMD driver enabled along
   with ``FLEXRAN SDK`` Libraries. Refer to *SW Turbo Poll Mode Driver*
   documentation for more details on this.

#. Go to the example directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/bbdev_app

#. Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linux-gcc

    See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#. Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

The application accepts a number of command line options:

.. code-block:: console

    $ ./build/bbdev [EAL options] -- [-e ENCODING_CORES] [-d DECODING_CORES] /
    [-p ETH_PORT_ID] [-b BBDEV_ID]

where:

* ``e ENCODING_CORES``: hexmask for encoding lcores (default = 0x2)
* ``d DECODING_CORES``: hexmask for decoding lcores (default = 0x4)
* ``p ETH_PORT_ID``: ethernet port ID (default = 0)
* ``b BBDEV_ID``: BBDev ID (default = 0)

The application requires that baseband devices is capable of performing
the specified baseband operation are available on application initialization.
This means that HW baseband device/s must be bound to a DPDK driver or
a SW baseband device/s (virtual BBdev) must be created (using --vdev).

To run the application in linux environment with the turbo_sw baseband device
using the whitelisted port running on 1 encoding lcore and 1 decoding lcore
issue the command:

.. code-block:: console

    $ ./build/bbdev --vdev='baseband_turbo_sw' -w <NIC0PCIADDR> -c 0x38 --socket-mem=2,2 \
    --file-prefix=bbdev -- -e 0x10 -d 0x20

where, NIC0PCIADDR is the PCI address of the Rx port

This command creates one virtual bbdev devices ``baseband_turbo_sw`` where the
device gets linked to a corresponding ethernet port as whitelisted by
the parameter -w.
3 cores are allocated to the application, and assigned as:

 - core 3 is the master and used to print the stats live on screen,

 - core 4 is the encoding lcore performing Rx and Turbo Encode operations

 - core 5 is the downlink lcore performing Turbo Decode, validation and Tx
   operations


Refer to the *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.

Using Packet Generator with baseband device sample application
--------------------------------------------------------------

To allow the bbdev sample app to do the loopback, an influx of traffic is required.
This can be done by using DPDK Pktgen to burst traffic on two ethernet ports, and
it will print the transmitted along with the looped-back traffic on Rx ports.
Executing the command below will generate traffic on the two whitelisted ethernet
ports.

.. code-block:: console

    $ ./pktgen-3.4.0/app/x86_64-native-linux-gcc/pktgen -c 0x3 \
    --socket-mem=1,1 --file-prefix=pg -w <NIC1PCIADDR> -- -m 1.0 -P

where:

* ``-c COREMASK``: A hexadecimal bitmask of cores to run on
* ``--socket-mem``: Memory to allocate on specific sockets (use comma separated values)
* ``--file-prefix``: Prefix for hugepage filenames
* ``-w <NIC1PCIADDR>``: Add a PCI device in white list. The argument format is <[domain:]bus:devid.func>.
* ``-m <string>``: Matrix for mapping ports to logical cores.
* ``-P``: PROMISCUOUS mode


Refer to *The Pktgen Application* documents for general information on running
Pktgen with DPDK applications.
