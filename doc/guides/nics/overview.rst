..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016 6WIND S.A.

Overview of Networking Drivers
==============================

The networking drivers may be classified in two categories:

- physical for real devices
- virtual for emulated devices

Some physical devices may be shaped through a virtual layer as for
SR-IOV.
The interface seen in the virtual environment is a VF (Virtual Function).

The ethdev layer exposes an API to use the networking functions
of these devices.
The bottom half part of ethdev is implemented by the drivers.
Thus some features may not be implemented.

There are more differences between drivers regarding some internal properties,
portability or even documentation availability.
Most of these differences are summarized below.

More details about features can be found in :doc:`features`.

.. _table_net_pmd_features:

.. include:: overview_table.txt

.. Note::

   Features marked with "P" are partially supported. Refer to the appropriate
   NIC guide in the following sections for details.

.. include:: rte_flow_items_table.txt

.. include:: rte_flow_actions_table.txt

.. Note::

   rte_flow actions marked with "I" can be indirect as well.
