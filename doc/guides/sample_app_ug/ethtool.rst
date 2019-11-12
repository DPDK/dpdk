..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

Ethtool Sample Application
==========================

The Ethtool sample application shows an implementation of an
ethtool-like API and provides a console environment that allows
its use to query and change Ethernet card parameters. The sample
is based upon a simple L2 frame reflector.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``ethtool`` sub-directory.

Running the Application
-----------------------

The application requires an available core for each port, plus one.
The only available options are the standard ones for the EAL:

.. code-block:: console

    ./ethtool-app/ethtool-app/${RTE_TARGET}/ethtool [EAL options]

Refer to the *DPDK Getting Started Guide* for general information on
running applications and the Environment Abstraction Layer (EAL)
options.

Using the application
---------------------

The application is console-driven using the cmdline DPDK interface:

.. code-block:: console

        EthApp>

From this interface the available commands and descriptions of what
they do as follows:

* ``drvinfo``: Print driver info
* ``eeprom``: Dump EEPROM to file
* ``module-eeprom``: Dump plugin module EEPROM to file
* ``link``: Print port link states
* ``macaddr``: Gets/sets MAC address
* ``mtu``: Set NIC MTU
* ``open``: Open port
* ``pause``: Get/set port pause state
* ``portstats``: Print port statistics
* ``regs``: Dump port register(s) to file
* ``ringparam``: Get/set ring parameters
* ``rxmode``: Toggle port Rx mode
* ``stop``: Stop port
* ``validate``: Check that given MAC address is valid unicast address
* ``vlan``: Add/remove VLAN id
* ``quit``: Exit program


Explanation
-----------

The sample program has two parts: A background `packet reflector`_
that runs on a slave core, and a foreground `Ethtool Shell`_ that
runs on the master core. These are described below.

Packet Reflector
~~~~~~~~~~~~~~~~

The background packet reflector is intended to demonstrate basic
packet processing on NIC ports controlled by the Ethtool shim.
Each incoming MAC frame is rewritten so that it is returned to
the sender, using the port in question's own MAC address as the
source address, and is then sent out on the same port.

Ethtool Shell
~~~~~~~~~~~~~

The foreground part of the Ethtool sample is a console-based
interface that accepts commands as described in `using the
application`_. Individual call-back functions handle the detail
associated with each command, which make use of the functions
defined in the `Ethtool interface`_ to the DPDK functions.

Ethtool interface
-----------------

The Ethtool interface is built as a separate library, and implements
the following functions:

- ``rte_ethtool_get_drvinfo()``
- ``rte_ethtool_get_regs_len()``
- ``rte_ethtool_get_regs()``
- ``rte_ethtool_get_link()``
- ``rte_ethtool_get_eeprom_len()``
- ``rte_ethtool_get_eeprom()``
- ``rte_ethtool_set_eeprom()``
- ``rte_ethtool_get_module_info()``
- ``rte_ethtool_get_module_eeprom()``
- ``rte_ethtool_get_pauseparam()``
- ``rte_ethtool_set_pauseparam()``
- ``rte_ethtool_net_open()``
- ``rte_ethtool_net_stop()``
- ``rte_ethtool_net_get_mac_addr()``
- ``rte_ethtool_net_set_mac_addr()``
- ``rte_ethtool_net_validate_addr()``
- ``rte_ethtool_net_change_mtu()``
- ``rte_ethtool_net_get_stats64()``
- ``rte_ethtool_net_vlan_rx_add_vid()``
- ``rte_ethtool_net_vlan_rx_kill_vid()``
- ``rte_ethtool_net_set_rx_mode()``
- ``rte_ethtool_get_ringparam()``
- ``rte_ethtool_set_ringparam()``
