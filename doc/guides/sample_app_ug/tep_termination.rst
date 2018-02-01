..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

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
becomes, the ability to offload overlay support features becomes an important requirement.
Intel® XL710 10/40 Gigabit Ethernet network card provides hardware filtering
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

The following figure shows the framework of the TEP termination sample
application based on DPDK vhost lib.

.. _figure_tep_termination_arch:

.. figure:: img/tep_termination_arch.*

   TEP termination Framework Overview

Supported Distributions
-----------------------

The example in this section have been validated with the following distributions:

*   Fedora* 18

*   Fedora* 19

*   Fedora* 20

Compiling the Sample Code
-------------------------

To enable vhost, turn on vhost library in the configure file
``config/common_linuxapp``.

    .. code-block:: console

        CONFIG_RTE_LIBRTE_VHOST=y

Then following the to compile the sample application shown in
:doc:`compiling`.

Running the Sample Code
-----------------------

#.  Go to the examples directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/tep_termination

#.  Run the tep_termination sample code:

    .. code-block:: console

        user@target:~$ ./build/app/tep_termination -l 0-3 -n 4 --huge-dir /mnt/huge --
                        -p 0x1 --dev-basename tep-termination --nb-devices 4
                        --udp-port 4789 --filter-type 1

.. note::

    Please note the huge-dir parameter instructs the DPDK to allocate its memory from the 2 MB page hugetlbfs.

Parameters
~~~~~~~~~~

**The same parameters with the vhost sample.**

Refer to :ref:`vhost_app_parameters` for detailed explanation.

**Number of Devices.**

The nb-devices option specifies the number of virtIO device.
The default value is 2.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -l 0-3 -n 4 --huge-dir /mnt/huge --
                    --nb-devices 2

**Tunneling UDP port.**

The udp-port option is used to specify the destination UDP number for UDP tunneling packet.
The default value is 4789.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -l 0-3 -n 4 --huge-dir /mnt/huge --
                    --nb-devices 2 --udp-port 4789

**Filter Type.**

The filter-type option is used to specify which filter type is used to
filter UDP tunneling packet to a specified queue.
The default value is 1, which means the filter type of inner MAC and tenant ID is used.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -l 0-3 -n 4 --huge-dir /mnt/huge --
                --nb-devices 2 --udp-port 4789 --filter-type 1

**TX Checksum.**

The tx-checksum option is used to enable or disable the inner header checksum offload.
The default value is 0, which means the checksum offload is disabled.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -l 0-3 -n 4 --huge-dir /mnt/huge --
                --nb-devices 2 --tx-checksum

**TCP segment size.**

The tso-segsz option specifies the TCP segment size for TSO offload for tunneling packet.
The default value is 0, which means TSO offload is disabled.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -l 0-3 -n 4 --huge-dir /mnt/huge --
                --tx-checksum --tso-segsz 800

**Decapsulation option.**

The decap option is used to enable or disable decapsulation operation for received VXLAN packet.
The default value is 1.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -l 0-3 -n 4 --huge-dir /mnt/huge --
                --nb-devices 4 --udp-port 4789 --decap 1

**Encapsulation option.**

The encap option is used to enable or disable encapsulation operation for transmitted packet.
The default value is 1.

.. code-block:: console

    user@target:~$ ./build/app/tep_termination -l 0-3 -n 4 --huge-dir /mnt/huge --
                --nb-devices 4 --udp-port 4789 --encap 1


Running the Virtual Machine (QEMU)
----------------------------------

Refer to :ref:`vhost_app_run_vm`.

Running DPDK in the Virtual Machine
-----------------------------------

Refer to :ref:`vhost_app_run_dpdk_inside_guest`.

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
