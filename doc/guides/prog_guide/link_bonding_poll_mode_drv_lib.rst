..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

Link Bonding Poll Mode Driver Library
=====================================

In addition to Poll Mode Drivers (PMDs) for physical and virtual hardware,
DPDK also includes a pure-software library that
allows physical PMDs to be bonded together to create a single logical PMD.

.. figure:: img/bond-overview.*

   Bonded PMDs


The Link Bonding PMD library(librte_net_bond) supports bonding of groups of
``rte_eth_dev`` ports of the same speed and duplex to provide similar
capabilities to that found in Linux bonding driver to allow the aggregation
of multiple (slave) NICs into a single logical interface between a server
and a switch. The new bonded PMD will then process these interfaces based on
the mode of operation specified to provide support for features such as
redundant links, fault tolerance and/or load balancing.

The librte_net_bond library exports a C API which provides an API for the
creation of bonded devices as well as the configuration and management of the
bonded device and its slave devices.

.. note::

    The Link Bonding PMD Library is enabled by default in the build
    configuration, the library can be disabled using the meson option
    "-Ddisable_drivers=net/bonding".


Link Bonding Modes Overview
---------------------------

Currently the Link Bonding PMD library supports following modes of operation:

*   **Round-Robin (Mode 0):**

.. figure:: img/bond-mode-0.*

   Round-Robin (Mode 0)


    This mode provides load balancing and fault tolerance by transmission of
    packets in sequential order from the first available slave device through
    the last. Packets are bulk dequeued from devices then serviced in a
    round-robin manner. This mode does not guarantee in order reception of
    packets and down stream should be able to handle out of order packets.

*   **Active Backup (Mode 1):**

.. figure:: img/bond-mode-1.*

   Active Backup (Mode 1)


    In this mode only one slave in the bond is active at any time, a different
    slave becomes active if, and only if, the primary active slave fails,
    thereby providing fault tolerance to slave failure. The single logical
    bonded interface's MAC address is externally visible on only one NIC (port)
    to avoid confusing the network switch.

*   **Balance XOR (Mode 2):**

.. figure:: img/bond-mode-2.*

   Balance XOR (Mode 2)


    This mode provides transmit load balancing (based on the selected
    transmission policy) and fault tolerance. The default policy (layer2) uses
    a simple calculation based on the packet flow source and destination MAC
    addresses as well as the number of active slaves available to the bonded
    device to classify the packet to a specific slave to transmit on. Alternate
    transmission policies supported are layer 2+3, this takes the IP source and
    destination addresses into the calculation of the transmit slave port and
    the final supported policy is layer 3+4, this uses IP source and
    destination addresses as well as the TCP/UDP source and destination port.

.. note::
    The coloring differences of the packets are used to identify different flow
    classification calculated by the selected transmit policy


*   **Broadcast (Mode 3):**

.. figure:: img/bond-mode-3.*

   Broadcast (Mode 3)


    This mode provides fault tolerance by transmission of packets on all slave
    ports.

*   **Link Aggregation 802.3AD (Mode 4):**

.. figure:: img/bond-mode-4.*

   Link Aggregation 802.3AD (Mode 4)


    This mode provides dynamic link aggregation according to the 802.3ad
    specification. It negotiates and monitors aggregation groups that share the
    same speed and duplex settings using the selected balance transmit policy
    for balancing outgoing traffic.

    DPDK implementation of this mode provide some additional requirements of
    the application.

    #. It needs to call ``rte_eth_tx_burst`` and ``rte_eth_rx_burst`` with
       intervals period of less than 100ms.

    #. Calls to ``rte_eth_tx_burst`` must have a buffer size of at least 2xN,
       where N is the number of slaves. This is a space required for LACP
       frames. Additionally LACP packets are included in the statistics, but
       they are not returned to the application.

*   **Transmit Load Balancing (Mode 5):**

.. figure:: img/bond-mode-5.*

   Transmit Load Balancing (Mode 5)


    This mode provides an adaptive transmit load balancing. It dynamically
    changes the transmitting slave, according to the computed load. Statistics
    are collected in 100ms intervals and scheduled every 10ms.


Implementation Details
----------------------

The librte_net_bond bonded device are compatible with the Ethernet device API
exported by the Ethernet PMDs described in the *DPDK API Reference*.

The Link Bonding Library supports the creation of bonded devices at application
startup time during EAL initialization using the ``--vdev`` option as well as
programmatically via the C API ``rte_eth_bond_create`` function.

Bonded devices support the dynamical addition and removal of slave devices using
the ``rte_eth_bond_slave_add`` / ``rte_eth_bond_slave_remove`` APIs.

After a slave device is added to a bonded device slave is stopped using
``rte_eth_dev_stop`` and then reconfigured using ``rte_eth_dev_configure``
the RX and TX queues are also reconfigured using ``rte_eth_tx_queue_setup`` /
``rte_eth_rx_queue_setup`` with the parameters use to configure the bonding
device. If RSS is enabled for bonding device, this mode is also enabled on new
slave and configured as well.
Any flow which was configured to the bond device also is configured to the added
slave.

Setting up multi-queue mode for bonding device to RSS, makes it fully
RSS-capable, so all slaves are synchronized with its configuration. This mode is
intended to provide RSS configuration on slaves transparent for client
application implementation.

Bonding device stores its own version of RSS settings i.e. RETA, RSS hash
function and RSS key, used to set up its slaves. That let to define the meaning
of RSS configuration of bonding device as desired configuration of whole bonding
(as one unit), without pointing any of slave inside. It is required to ensure
consistency and made it more error-proof.

RSS hash function set for bonding device, is a maximal set of RSS hash functions
supported by all bonded slaves. RETA size is a GCD of all its RETA's sizes, so
it can be easily used as a pattern providing expected behavior, even if slave
RETAs' sizes are different. If RSS Key is not set for bonded device, it's not
changed on the slaves and default key for device is used.

As RSS configurations, there is flow consistency in the bonded slaves for the
next rte flow operations:

Validate:
	- Validate flow for each slave, failure at least for one slave causes to
	  bond validation failure.

Create:
	- Create the flow in all slaves.
	- Save all the slaves created flows objects in bonding internal flow
	  structure.
	- Failure in flow creation for existed slave rejects the flow.
	- Failure in flow creation for new slaves in slave adding time rejects
	  the slave.

Destroy:
	- Destroy the flow in all slaves and release the bond internal flow
	  memory.

Flush:
	- Destroy all the bonding PMD flows in all the slaves.

.. note::

    Don't call slaves flush directly, It destroys all the slave flows which
    may include external flows or the bond internal LACP flow.

Query:
	- Summarize flow counters from all the slaves, relevant only for
	  ``RTE_FLOW_ACTION_TYPE_COUNT``.

Isolate:
	- Call to flow isolate for all slaves.
	- Failure in flow isolation for existed slave rejects the isolate mode.
	- Failure in flow isolation for new slaves in slave adding time rejects
	  the slave.

All settings are managed through the bonding port API and always are propagated
in one direction (from bonding to slaves).

Link Status Change Interrupts / Polling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Link bonding devices support the registration of a link status change callback,
using the ``rte_eth_dev_callback_register`` API, this will be called when the
status of the bonding device changes. For example in the case of a bonding
device which has 3 slaves, the link status will change to up when one slave
becomes active or change to down when all slaves become inactive. There is no
callback notification when a single slave changes state and the previous
conditions are not met. If a user wishes to monitor individual slaves then they
must register callbacks with that slave directly.

The link bonding library also supports devices which do not implement link
status change interrupts, this is achieved by polling the devices link status at
a defined period which is set using the ``rte_eth_bond_link_monitoring_set``
API, the default polling interval is 10ms. When a device is added as a slave to
a bonding device it is determined using the ``RTE_PCI_DRV_INTR_LSC`` flag
whether the device supports interrupts or whether the link status should be
monitored by polling it.

Requirements / Limitations
~~~~~~~~~~~~~~~~~~~~~~~~~~

The current implementation only supports devices that support the same speed
and duplex to be added as a slaves to the same bonded device. The bonded device
inherits these attributes from the first active slave added to the bonded
device and then all further slaves added to the bonded device must support
these parameters.

A bonding device must have a minimum of one slave before the bonding device
itself can be started.

To use a bonding device dynamic RSS configuration feature effectively, it is
also required, that all slaves should be RSS-capable and support, at least one
common hash function available for each of them. Changing RSS key is only
possible, when all slave devices support the same key size.

To prevent inconsistency on how slaves process packets, once a device is added
to a bonding device, RSS and rte flow configurations should be managed through
the bonding device API, and not directly on the slave.

Like all other PMD, all functions exported by a PMD are lock-free functions
that are assumed not to be invoked in parallel on different logical cores to
work on the same target object.

It should also be noted that the PMD receive function should not be invoked
directly on a slave devices after they have been to a bonded device since
packets read directly from the slave device will no longer be available to the
bonded device to read.

Configuration
~~~~~~~~~~~~~

Link bonding devices are created using the ``rte_eth_bond_create`` API
which requires a unique device name, the bonding mode,
and the socket Id to allocate the bonding device's resources on.
The other configurable parameters for a bonded device are its slave devices,
its primary slave, a user defined MAC address and transmission policy to use if
the device is in balance XOR mode.

Slave Devices
^^^^^^^^^^^^^

Bonding devices support up to a maximum of ``RTE_MAX_ETHPORTS`` slave devices
of the same speed and duplex. Ethernet devices can be added as a slave to a
maximum of one bonded device. Slave devices are reconfigured with the
configuration of the bonded device on being added to a bonded device.

The bonded also guarantees to return the MAC address of the slave device to its
original value of removal of a slave from it.

Primary Slave
^^^^^^^^^^^^^

The primary slave is used to define the default port to use when a bonded
device is in active backup mode. A different port will only be used if, and
only if, the current primary port goes down. If the user does not specify a
primary port it will default to being the first port added to the bonded device.

MAC Address
^^^^^^^^^^^

The bonded device can be configured with a user specified MAC address, this
address will be inherited by the some/all slave devices depending on the
operating mode. If the device is in active backup mode then only the primary
device will have the user specified MAC, all other slaves will retain their
original MAC address. In mode 0, 2, 3, 4 all slaves devices are configure with
the bonded devices MAC address.

If a user defined MAC address is not defined then the bonded device will
default to using the primary slaves MAC address.

Balance XOR Transmit Policies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

There are 3 supported transmission policies for bonded device running in
Balance XOR mode. Layer 2, Layer 2+3, Layer 3+4.

*   **Layer 2:**   Ethernet MAC address based balancing is the default
    transmission policy for Balance XOR bonding mode. It uses a simple XOR
    calculation on the source MAC address and destination MAC address of the
    packet and then calculate the modulus of this value to calculate the slave
    device to transmit the packet on.

*   **Layer 2 + 3:** Ethernet MAC address & IP Address based balancing uses a
    combination of source/destination MAC addresses and the source/destination
    IP addresses of the data packet to decide which slave port the packet will
    be transmitted on.

*   **Layer 3 + 4:**  IP Address & UDP Port based  balancing uses a combination
    of source/destination IP Address and the source/destination UDP ports of
    the packet of the data packet to decide which slave port the packet will be
    transmitted on.

All these policies support 802.1Q VLAN Ethernet packets, as well as IPv4, IPv6
and UDP protocols for load balancing.

Using Link Bonding Devices
--------------------------

The librte_net_bond library supports two modes of device creation, the libraries
export full C API or using the EAL command line to statically configure link
bonding devices at application startup. Using the EAL option it is possible to
use link bonding functionality transparently without specific knowledge of the
libraries API, this can be used, for example, to add bonding functionality,
such as active backup, to an existing application which has no knowledge of
the link bonding C API.

Using the Poll Mode Driver from an Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Using the librte_net_bond libraries API it is possible to dynamically create
and manage link bonding device from within any application. Link bonding
devices are created using the ``rte_eth_bond_create`` API which requires a
unique device name, the link bonding mode to initial the device in and finally
the socket Id which to allocate the devices resources onto. After successful
creation of a bonding device it must be configured using the generic Ethernet
device configure API ``rte_eth_dev_configure`` and then the RX and TX queues
which will be used must be setup using ``rte_eth_tx_queue_setup`` /
``rte_eth_rx_queue_setup``.

Slave devices can be dynamically added and removed from a link bonding device
using the ``rte_eth_bond_slave_add`` / ``rte_eth_bond_slave_remove``
APIs but at least one slave device must be added to the link bonding device
before it can be started using ``rte_eth_dev_start``.

The link status of a bonded device is dictated by that of its slaves, if all
slave device link status are down or if all slaves are removed from the link
bonding device then the link status of the bonding device will go down.

It is also possible to configure / query the configuration of the control
parameters of a bonded device using the provided APIs
``rte_eth_bond_mode_set/ get``, ``rte_eth_bond_primary_set/get``,
``rte_eth_bond_mac_set/reset`` and ``rte_eth_bond_xmit_policy_set/get``.

Using Link Bonding Devices from the EAL Command Line
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Link bonding devices can be created at application startup time using the
``--vdev`` EAL command line option. The device name must start with the
net_bonding prefix followed by numbers or letters. The name must be unique for
each device. Each device can have multiple options arranged in a comma
separated list. Multiple devices definitions can be arranged by calling the
``--vdev`` option multiple times.

Device names and bonding options must be separated by commas as shown below:

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -l 0-3 -n 4 --vdev 'net_bonding0,bond_opt0=..,bond opt1=..'--vdev 'net_bonding1,bond _opt0=..,bond_opt1=..'

Link Bonding EAL Options
^^^^^^^^^^^^^^^^^^^^^^^^

There are multiple ways of definitions that can be assessed and combined as
long as the following two rules are respected:

*   A unique device name, in the format of net_bondingX is provided,
    where X can be any combination of numbers and/or letters,
    and the name is no greater than 32 characters long.

*   A least one slave device is provided with for each bonded device definition.

*   The operation mode of the bonded device being created is provided.

The different options are:

*   mode: Integer value defining the bonding mode of the device.
    Currently supports modes 0,1,2,3,4,5 (round-robin, active backup, balance,
    broadcast, link aggregation, transmit load balancing).

.. code-block:: console

        mode=2

*   slave: Defines the PMD device which will be added as slave to the bonded
    device. This option can be selected multiple times, for each device to be
    added as a slave. Physical devices should be specified using their PCI
    address, in the format domain:bus:devid.function

.. code-block:: console

        slave=0000:0a:00.0,slave=0000:0a:00.1

*   primary: Optional parameter which defines the primary slave port,
    is used in active backup mode to select the primary slave for data TX/RX if
    it is available. The primary port also is used to select the MAC address to
    use when it is not defined by the user. This defaults to the first slave
    added to the device if it is specified. The primary device must be a slave
    of the bonded device.

.. code-block:: console

        primary=0000:0a:00.0

*   socket_id: Optional parameter used to select which socket on a NUMA device
    the bonded devices resources will be allocated on.

.. code-block:: console

        socket_id=0

*   mac: Optional parameter to select a MAC address for link bonding device,
    this overrides the value of the primary slave device.

.. code-block:: console

        mac=00:1e:67:1d:fd:1d

*   xmit_policy: Optional parameter which defines the transmission policy when
    the bonded device is in  balance mode. If not user specified this defaults
    to l2 (layer 2) forwarding, the other transmission policies available are
    l23 (layer 2+3) and l34 (layer 3+4)

.. code-block:: console

        xmit_policy=l23

*   lsc_poll_period_ms: Optional parameter which defines the polling interval
    in milli-seconds at which devices which don't support lsc interrupts are
    checked for a change in the devices link status

.. code-block:: console

        lsc_poll_period_ms=100

*   up_delay: Optional parameter which adds a delay in milli-seconds to the
    propagation of a devices link status changing to up, by default this
    parameter is zero.

.. code-block:: console

        up_delay=10

*   down_delay: Optional parameter which adds a delay in milli-seconds to the
    propagation of a devices link status changing to down, by default this
    parameter is zero.

.. code-block:: console

        down_delay=50

Examples of Usage
^^^^^^^^^^^^^^^^^

Create a bonded device in round robin mode with two slaves specified by their PCI address:

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -l 0-3 -n 4 --vdev 'net_bonding0,mode=0,slave=0000:0a:00.01,slave=0000:04:00.00' -- --port-topology=chained

Create a bonded device in round robin mode with two slaves specified by their PCI address and an overriding MAC address:

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -l 0-3 -n 4 --vdev 'net_bonding0,mode=0,slave=0000:0a:00.01,slave=0000:04:00.00,mac=00:1e:67:1d:fd:1d' -- --port-topology=chained

Create a bonded device in active backup mode with two slaves specified, and a primary slave specified by their PCI addresses:

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -l 0-3 -n 4 --vdev 'net_bonding0,mode=1,slave=0000:0a:00.01,slave=0000:04:00.00,primary=0000:0a:00.01' -- --port-topology=chained

Create a bonded device in balance mode with two slaves specified by their PCI addresses, and a transmission policy of layer 3 + 4 forwarding:

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -l 0-3 -n 4 --vdev 'net_bonding0,mode=2,slave=0000:0a:00.01,slave=0000:04:00.00,xmit_policy=l34' -- --port-topology=chained

.. _bonding_testpmd_commands:

Testpmd driver specific commands
--------------------------------

Some bonding driver specific features are integrated in testpmd.

create bonded device
~~~~~~~~~~~~~~~~~~~~

Create a new bonding device::

   testpmd> create bonded device (mode) (socket)

For example, to create a bonded device in mode 1 on socket 0::

   testpmd> create bonded device 1 0
   created new bonded device (port X)

add bonding slave
~~~~~~~~~~~~~~~~~

Adds Ethernet device to a Link Bonding device::

   testpmd> add bonding slave (slave id) (port id)

For example, to add Ethernet device (port 6) to a Link Bonding device (port 10)::

   testpmd> add bonding slave 6 10


remove bonding slave
~~~~~~~~~~~~~~~~~~~~

Removes an Ethernet slave device from a Link Bonding device::

   testpmd> remove bonding slave (slave id) (port id)

For example, to remove Ethernet slave device (port 6) to a Link Bonding device (port 10)::

   testpmd> remove bonding slave 6 10

set bonding mode
~~~~~~~~~~~~~~~~

Set the Link Bonding mode of a Link Bonding device::

   testpmd> set bonding mode (value) (port id)

For example, to set the bonding mode of a Link Bonding device (port 10) to broadcast (mode 3)::

   testpmd> set bonding mode 3 10

set bonding primary
~~~~~~~~~~~~~~~~~~~

Set an Ethernet slave device as the primary device on a Link Bonding device::

   testpmd> set bonding primary (slave id) (port id)

For example, to set the Ethernet slave device (port 6) as the primary port of a Link Bonding device (port 10)::

   testpmd> set bonding primary 6 10

set bonding mac
~~~~~~~~~~~~~~~

Set the MAC address of a Link Bonding device::

   testpmd> set bonding mac (port id) (mac)

For example, to set the MAC address of a Link Bonding device (port 10) to 00:00:00:00:00:01::

   testpmd> set bonding mac 10 00:00:00:00:00:01

set bonding balance_xmit_policy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the transmission policy for a Link Bonding device when it is in Balance XOR mode::

   testpmd> set bonding balance_xmit_policy (port_id) (l2|l23|l34)

For example, set a Link Bonding device (port 10) to use a balance policy of layer 3+4 (IP addresses & UDP ports)::

   testpmd> set bonding balance_xmit_policy 10 l34


set bonding mon_period
~~~~~~~~~~~~~~~~~~~~~~

Set the link status monitoring polling period in milliseconds for a bonding device.

This adds support for PMD slave devices which do not support link status interrupts.
When the mon_period is set to a value greater than 0 then all PMD's which do not support
link status ISR will be queried every polling interval to check if their link status has changed::

   testpmd> set bonding mon_period (port_id) (value)

For example, to set the link status monitoring polling period of bonded device (port 5) to 150ms::

   testpmd> set bonding mon_period 5 150


set bonding lacp dedicated_queue
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enable dedicated tx/rx queues on bonding devices slaves to handle LACP control plane traffic
when in mode 4 (link-aggregation-802.3ad)::

   testpmd> set bonding lacp dedicated_queues (port_id) (enable|disable)


set bonding agg_mode
~~~~~~~~~~~~~~~~~~~~

Enable one of the specific aggregators mode when in mode 4 (link-aggregation-802.3ad)::

   testpmd> set bonding agg_mode (port_id) (bandwidth|count|stable)


show bonding config
~~~~~~~~~~~~~~~~~~~

Show the current configuration of a Link Bonding device,
it also shows link-aggregation-802.3ad information if the link mode is mode 4::

   testpmd> show bonding config (port id)

For example,
to show the configuration a Link Bonding device (port 9) with 3 slave devices (1, 3, 4)
in balance mode with a transmission policy of layer 2+3::

   testpmd> show bonding config 9
     - Dev basic:
        Bonding mode: BALANCE(2)
        Balance Xmit Policy: BALANCE_XMIT_POLICY_LAYER23
        Slaves (3): [1 3 4]
        Active Slaves (3): [1 3 4]
        Primary: [3]
