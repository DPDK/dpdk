
..  BSD LICENSE
    Copyright(c) 2015 Intel Corporation. All rights reserved.
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

Ethtool Sample Application
==========================

The Ethtool sample application shows an implementation of an
ethtool-like API and provides a console environment that allows
its use to query and change Ethernet card parameters. The sample
is based upon a simple L2 frame reflector.

Compiling the Application
-------------------------

To compile the application:

#.  Go to the sample application directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SD}/examples/ethtool

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

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
they do as as follows:

* ``drvinfo``: Print driver info
* ``eeprom``: Dump EEPROM to file
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
