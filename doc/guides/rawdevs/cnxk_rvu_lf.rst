.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2024 Marvell.

Marvell CNXK RVU LF Driver
==========================

CNXK product families can have a use case to allow RVU PF and RVU VF
driver to communicate using mailboxes
and also get notified of any interrupt that may occur on the device.
Hence, a new raw device driver is added for such RVU LF devices.
These devices can map to a RVU PF or a RVU VF
which can send mailboxes to each other.

Features
--------

The RVU LF device implements following features in the rawdev API:

- Get PF FUNC of associated NPA and SSO devices.

Limitations
-----------

In multi-process mode user-space application must ensure
no resources sharing takes place.
Otherwise, user-space application should ensure synchronization.

Device Setup
------------

The RVU LF devices will need to be bound to a user-space IO driver for use.
The script ``dpdk-devbind.py`` included with DPDK can be used
to view the state of the devices
and to bind them to a suitable DPDK-supported kernel driver.
When querying the status of the devices,
they will appear under the category of "Misc (rawdev) devices",
i.e. the command ``dpdk-devbind.py --status-dev misc``
can be used to see the state of those devices alone.

Get NPA and SSO PF FUNC
-----------------------

API functions ``rte_pmd_rvu_lf_npa_pf_func_get()`` and ``rte_pmd_rvu_lf_sso_pf_func_get()``
can be used to get the cnxk NPA PF func and SSO PF func
which application can use for NPA/SSO specific configuration.
