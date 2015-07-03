
..  BSD LICENSE
    Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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


TEP termination Sample Application
==================================

The TEP (Tunnel End point) termination sample application simulates a VXLAN
Tunnel Endpoint (VTEP) termination in DPDK, which is used to demonstrate
the offload and filtering capabilities of Intel® XL710 10/40 Gigabit Ethernet
Controller for VXLAN packet.
This sample uses the basic virtio devices management mechanism from vhost example,
and also uses the us-vHost interface and tunnel filtering mechanism to direct
a specified traffic to a specific VM.
In addition, this sample is also designed to show how tunneling protocols can be handled.

Background
----------

With virtualization, overlay networks allow a network structure to be built
or imposed across physical nodes which is abstracted away from the actual
underlining physical network connections.
This allows network isolation, QOS, etc to be provided on a per client basis.

.. _figure_overlay_networking:

.. figure:: img/overlay_networking.*

   Overlay Networking.

In a typical setup, the network overlay tunnel is terminated at the Virtual/Tunnel End Point (VEP/TEP).
The TEP is normally located at the physical host level ideally in the software switch.
Due to processing constraints and the inevitable bottleneck that the switch
becomes the ability to offload overlay support features becomes an important requirement.
Intel® XL710 10/40 G Ethernet network card provides hardware filtering
and offload capabilities to support overlay networks implementations such as MAC in UDP and MAC in GRE.

Sample Code Overview
--------------------

The DPDK TEP termination sample code demonstrates the offload and filtering
capabilities of Intel® XL710 10/40 Gigabit Ethernet Controller for VXLAN packet.

The sample code is based on vhost library.
The vhost library is developed for user space Ethernet switch to easily integrate with vhost functionality.

The sample will support the followings:

*   Tunneling packet recognition.

*   The port of UDP tunneling is configurable

*   Directing incoming traffic to the correct queue based on the tunnel filter type.
    The supported filter type are listed below.

    * Inner MAC and VLAN and tenant ID

    * Inner MAC and tenant ID, and Outer MAC

    * Inner MAC and tenant ID

    The tenant ID will be assigned from a static internal table based on the us-vhost device ID.
    Each device will receive a unique device ID.
    The inner MAC will be learned by the first packet transmitted from a device.

*   Decapsulation of RX VXLAN traffic. This is a software only operation.

*   Encapsulation of TX VXLAN traffic. This is a software only operation.

*   Inner IP and inner L4 checksum offload.

*   TSO offload support for tunneling packet.

The following figure shows the framework of the TEP termination sample application based on vhost-cuse.

.. _figure_tep_termination_arch:

.. figure:: img/tep_termination_arch.*

   TEP termination Framework Overview

Supported Distributions
-----------------------

The example in this section have been validated with the following distributions:

*   Fedora* 18

*   Fedora* 19

*   Fedora* 20

Prerequisites
-------------

Refer to the guide in section 27.4 in the vhost sample.

Compiling the Sample Code
-------------------------
#.  Compile vhost lib:

    To enable vhost, turn on vhost library in the configure file config/common_linuxapp.

    .. code-block:: console

        CONFIG_RTE_LIBRTE_VHOST=n

    vhost user is turned on by default in the configure file config/common_linuxapp.
    To enable vhost cuse, disable vhost user.

    .. code-block:: console

        CONFIG_RTE_LIBRTE_VHOST_USER=y

     After vhost is enabled and the implementation is selected, build the vhost library.

#.  Go to the examples directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/tep_termination

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the DPDK Getting Started Guide for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        cd ${RTE_SDK}
        make config ${RTE_TARGET}
        make install ${RTE_TARGET}
        cd ${RTE_SDK}/examples/tep_termination
        make

#.  Go to the eventfd_link directory(vhost cuse required):

    .. code-block:: console

        cd ${RTE_SDK}/lib/librte_vhost/eventfd_link

#.  Build the eventfd_link kernel module(vhost cuse required):

    .. code-block:: console

        make

Running the Sample Code
-----------------------

#.  Install the cuse kernel module(vhost cuse required):

    .. code-block:: console

        modprobe cuse

#.  Go to the eventfd_link directory(vhost cuse required):

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/lib/librte_vhost/eventfd_link

#.  Install the eventfd_link module(vhost cuse required):

    .. code-block:: console

        insmod ./eventfd_link.ko

#.  Go to the examples directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/tep_termination

#.  Run the tep_termination sample code:

    .. code-block:: console

        user@target:~$ ./build/app/tep_termination -c f -n 4 --huge-dir /mnt/huge --
                        -p 0x1 --dev-basename tep-termination --nb-devices 4
                        --udp-port 4789 --filter-type 1

.. note::

    Please note the huge-dir parameter instructs the DPDK to allocate its memory from the 2 MB page hugetlbfs.

Parameters
~~~~~~~~~~

**The same parameters with the vhost sample.**

Refer to the guide in section 27.6.1 in the vhost sample for the meanings of 'Basename',
'Stats', 'RX Retry', 'RX Retry Number' and 'RX Retry Delay Time'.

**Number of Devices.**

The nb-devices option specifies the number of virtIO device.
The default value is 2.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -c f -n 4 --huge-dir /mnt/huge --
                    --nb-devices 2

**Tunneling UDP port.**

The udp-port option is used to specify the destination UDP number for UDP tunneling packet.
The default value is 4789.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -c f -n 4 --huge-dir /mnt/huge --
                    --nb-devices 2 --udp-port 4789

**Filter Type.**

The filter-type option is used to specify which filter type is used to
filter UDP tunneling packet to a specified queue.
The default value is 1, which means the filter type of inner MAC and tenant ID is used.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -c f -n 4 --huge-dir /mnt/huge --
                --nb-devices 2 --udp-port 4789 --filter-type 1

**TX Checksum.**

The tx-checksum option is used to enable or disable the inner header checksum offload.
The default value is 0, which means the checksum offload is disabled.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -c f -n 4 --huge-dir /mnt/huge --
                --nb-devices 2 --tx-checksum

**TCP segment sise.**

The tso-segsz option specifies the TCP segment size for TSO offload for tunneling packet.
The default value is 0, which means TSO offload is disabled.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -c f -n 4 --huge-dir /mnt/huge --
                --tx-checksum --tso-segsz 800

**Decapsulation option.**

The decap option is used to enable or disable decapsulation operation for received VXLAN packet.
The default value is 1.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -c f -n 4 --huge-dir /mnt/huge --
                --nb-devices 4 --udp-port 4789 --decap 1

**Encapsulation option.**

The encap option is used to enable or disable encapsulation operation for transmitted packet.
The default value is 1.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -c f -n 4 --huge-dir /mnt/huge --
                --nb-devices 4 --udp-port 4789 --encap 1


Running the Virtual Machine (QEMU)
----------------------------------

Refer to the guide in section 27.7 in the vhost sample.

Running DPDK in the Virtual Machine
-----------------------------------

Refer to the guide in section 27.8 in the vHost sample.

Passing Traffic to the Virtual Machine Device
---------------------------------------------

For a virtio-net device to receive traffic, the traffic's Layer 2 header must include
both the virtio-net device's MAC address.
The DPDK sample code behaves in a similar manner to a learning switch in that
it learns the MAC address of the virtio-net devices from the first transmitted packet.
On learning the MAC address,
the DPDK vhost sample code prints a message with the MAC address and tenant ID virtio-net device.
For example:

.. code-block:: console

    DATA: (0) MAC_ADDRESS cc:bb:bb:bb:bb:bb and VNI 1000 registered

The above message indicates that device 0 has been registered with MAC address cc:bb:bb:bb:bb:bb and VNI 1000.
Any packets received on the NIC with these values are placed on the devices receive queue.
